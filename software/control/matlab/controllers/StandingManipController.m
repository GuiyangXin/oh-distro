classdef StandingManipController < DRCController
  
  properties (SetAccess=protected,GetAccess=protected)
    robot;
    foot_idx;
    contact_est_monitor;
  end
  
  methods
  
    function obj = StandingManipController(name,r,options)
      typecheck(r,'Atlas');

      ctrl_data = SharedDataHandle(struct(...
        'A',[zeros(2),eye(2); zeros(2,4)],...
        'B',[zeros(2); eye(2)],...
        'C',[eye(2),zeros(2)],...
        'R',zeros(2),...
        'Qy',eye(2),...
        'is_time_varying',false,...
        'S',zeros(4),...
        's1',zeros(4,1),...
        's2',0,...
        'x0',zeros(4,1),...
        'u0',zeros(2,1),...
        'y0',zeros(2,1),...
        'support_times',0,...
        'supports',[],...
        'mu',1.0,...
        'trans_drift',[0;0;0],...
        'ignore_terrain',false,...
        'qtraj',zeros(getNumDOF(r),1),...
        'V',0,... % cost to go used in controller status message
        'Vdot',0,...
        'r_arm_control_type',drc.robot_plan_t.NONE,...
        'l_arm_control_type',drc.robot_plan_t.NONE));
      
      % instantiate QP controller
      options.slack_limit = 30.0;
      options.w = 0.001;
      options.lcm_foot_contacts = true;
			options.contact_threshold = 0.005;
      if(~isfield(options,'use_mex')) options.use_mex = false; end
      if(~isfield(options,'debug')) options.debug = false; end
      
      options.lcm_foot_contacts = true;
      qp = QPControlBlock(r,ctrl_data,options);

      % cascade PD qtraj controller 
			pd = SimplePDBlock(r,ctrl_data);
      ins(1).system = 1;
      ins(1).input = 1;
      ins(2).system = 1;
      ins(2).input = 2;
      ins(3).system = 2;
      ins(3).input = 3;
      outs(1).system = 2;
      outs(1).output = 1;
      sys = mimoCascade(pd,qp,[],ins,outs);
      clear connection ins outs;
      
      % cascade neck pitch control block
      neck = NeckControlBlock(r,ctrl_data);
      ins(1).system = 1;
      ins(1).input = 1;
      ins(2).system = 1;
      ins(2).input = 2;
      ins(3).system = 2;
      ins(3).input = 3;
      outs(1).system = 2;
      outs(1).output = 1;
      connection(1).from_output = 1;
      connection(1).to_input = 1;
      connection(2).from_output = 2;
      connection(2).to_input = 2;
      sys = mimoCascade(neck,sys,connection,ins,outs);

			% cascade cartesian controller on left and right hands
%       ee_control = ManipCartesianController(r,ctrl_data);
% 			ins(1).system = 1;
% 			ins(1).input = 1;
% 			ins(2).system = 1;
% 			ins(2).input = 2;
% 			ins(3).system = 1;
% 			ins(3).input = 3;
% 			ins(4).system = 2;
% 			ins(4).input = 2;
% 			outs(1).system = 2;
% 			outs(1).output = 1;
% 			sys = mimoCascade(sys,ee_control,[],ins,outs);

      obj = obj@DRCController(name,sys,AtlasState(r));
 
      obj.robot = r;
      obj.controller_data = ctrl_data;
      
      obj.contact_est_monitor = drake.util.MessageMonitor(drc.foot_contact_estimate_t,'utime');
      obj.lc.subscribe('FOOT_CONTACT_ESTIMATE',obj.contact_est_monitor);
      
      % use saved nominal pose 
      d = load(strcat(getenv('DRC_PATH'),'/control/matlab/data/atlas_fp.mat'));
      q0 = d.xstar(1:getNumDOF(obj.robot));
      kinsol = doKinematics(obj.robot,q0);
      com = getCOM(obj.robot,kinsol);

      % build TI-ZMP controller 
      foot_pos = contactPositions(obj.robot,kinsol); 
      ch = convhull(foot_pos(1:2,:)'); % assumes foot-only contact model
      comgoal = mean(foot_pos(1:2,ch(1:end-1)),2);
      limp = LinearInvertedPendulum(com(3));
      [~,V] = lqr(limp,comgoal);

      obj.foot_idx = [r.findLinkInd('r_foot'),r.findLinkInd('l_foot')];
      supports = SupportState(r,obj.foot_idx);
      
      obj.controller_data.setField('S',V.S);
      obj.controller_data.setField('D',-com(3)/9.81*eye(2));
      obj.controller_data.setField('qtraj',q0);
      obj.controller_data.setField('x0',[comgoal;0;0]);
      obj.controller_data.setField('y0',comgoal);
      obj.controller_data.setField('supports',supports);
      
      obj = addLCMTransition(obj,'WALKING_PLAN',drc.walking_plan_t(),'walking');
      obj = addLCMTransition(obj,'BRACE_FOR_FALL',drc.utime_t(),'bracing');

      % should make this a more specific channel name
      obj = addLCMTransition(obj,'COMMITTED_ROBOT_PLAN',drc.robot_plan_t(),name); % for standing/reaching tasks
      obj = addLCMTransition(obj,'RECOVERY_CMD',drc.recovery_t(),'dummy'); % for recovery procedure

    end
    
    function msg = status_message(obj,t_sim,t_ctrl)
        msg = drc.controller_status_t();
        msg.utime = t_sim * 1000000;
        msg.state = msg.STANDING;
        msg.controller_utime = t_ctrl * 1000000;
        msg.V = obj.controller_data.getField('V');
        msg.Vdot = obj.controller_data.getField('Vdot');
    end
    
    function obj = initialize(obj,data)

      
      if isfield(data,'STOP_WALKING')
        % transition from walking:
        % take in new nominal pose and compute standing controller

        % get foot contact state over LCM
        contact_data = obj.contact_est_monitor.getMessage();
        if isempty(contact_data)
          lfoot_contact_state = 1;
          rfoot_contact_state = 1;
        else
          msg = drc.foot_contact_estimate_t(contact_data);
          lfoot_contact_state = msg.left_contact > 0.5;
          rfoot_contact_state = msg.right_contact > 0.5;
        end
        
        r = obj.robot;
        q0 = data.AtlasState(1:getNumDOF(r));
        kinsol = doKinematics(r,q0);
%         com = getCOM(r,kinsol);

        foot_pos = contactPositions(r,kinsol,obj.foot_idx([rfoot_contact_state lfoot_contact_state])); 
        ch = convhull(foot_pos(1:2,:)');
        comgoal = mean(foot_pos(1:2,ch(1:end-1)),2);
%         foot_pos = contactPositions(r,kinsol,obj.foot_idx);
%         zfeet = mean(foot_pos(3,:));
%         robot_z = com(3)-zfeet;
  
%         obj.controller_data.setField('D',-robot_z/9.81*eye(2));
        obj.controller_data.setField('qtraj',q0);
        obj.controller_data.setField('x0',[comgoal;0;0]);
        obj.controller_data.setField('y0',comgoal);
        
      elseif isfield(data,'COMMITTED_ROBOT_PLAN')
        % standing and reaching plan
        %sprintf('standing controller on\n');
        try
          msg = data.COMMITTED_ROBOT_PLAN;
          joint_names = obj.robot.getStateFrame.coordinates(1:getNumDOF(obj.robot));
          [xtraj,ts,~,control_type] = RobotPlanListener.decodeRobotPlan(msg,true,joint_names); 
          qtraj = PPTrajectory(spline(ts,xtraj(1:getNumDOF(obj.robot),:)));

          obj.controller_data.setField('qtraj',qtraj);
          obj.controller_data.setField('r_arm_control_type',control_type.right_arm_control_type);
          obj.controller_data.setField('l_arm_control_type',control_type.left_arm_control_type);
          obj = setDuration(obj,inf,false); % set the controller timeout
        catch err
          r = obj.robot;

          x0 = data.AtlasState; % should have an atlas state
          q0 = x0(1:getNumDOF(r));
          kinsol = doKinematics(r,q0);

          foot_pos = contactPositions(r,kinsol,obj.foot_idx); 
          ch = convhull(foot_pos(1:2,:)');
          comgoal = mean(foot_pos(1:2,ch(1:end-1)),2);
          obj.controller_data.setField('qtraj',q0);
          obj.controller_data.setField('x0',[comgoal;0;0]);
          obj.controller_data.setField('y0',comgoal);
 
        end
        
      elseif isfield(data,'AtlasState')
        % transition from walking:
        % take in new nominal pose and compute standing controller
        r = obj.robot;

        x0 = data.AtlasState;
        q0 = x0(1:getNumDOF(r));
        kinsol = doKinematics(r,q0);
%         com = getCOM(r,kinsol);

        foot_pos = contactPositions(r,kinsol,obj.foot_idx); 
        ch = convhull(foot_pos(1:2,:)');
        comgoal = mean(foot_pos(1:2,ch(1:end-1)),2);
%         foot_pos = contactPositions(r,kinsol,obj.foot_idx);
%         zfeet = mean(foot_pos(3,:));
%         robot_z = com(3)-zfeet;
  
%         obj.controller_data.setField('D',-robot_z/9.81*eye(2));
        obj.controller_data.setField('qtraj',q0);
        obj.controller_data.setField('x0',[comgoal;0;0]);
        obj.controller_data.setField('y0',comgoal);
     
      
      else
        % first initialization should come here... wait for state
        state_frame = getStateFrame(obj.robot);
        state_frame.subscribe(state_frame.channel);
        while true
          [x,t] = getNextMessage(state_frame,10);
          if (~isempty(x) && t>6.0) % wait for 6 seconds at startup
            data = struct();
            data.AtlasState = x;
            break;
          end
        end
        obj = initialize(obj,data);
      end
     
      QPControlBlock.check_ctrl_data(obj.controller_data);  
      obj = setDuration(obj,inf,false); % set the controller timeout
    end
  end  
end
