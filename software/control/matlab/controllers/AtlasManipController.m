classdef AtlasManipController < DRCController
  
  properties (SetAccess=protected,GetAccess=protected)
    robot;
    arm_joints;
    back_joints;
  end
  
  methods
  
    function obj = AtlasManipController(name,r,options)
      typecheck(r,'Atlas');

      if nargin < 3
        options = struct();
      end
      
      if ~isfield(options,'controller_type')
        options.controller_type = 2;
      end
      
      arm_ind = ~cellfun(@isempty,strfind(r.getStateFrame.coordinates(1:getNumDOF(r)),'arm'));
      back_ind = ~cellfun(@isempty,strfind(r.getStateFrame.coordinates(1:getNumDOF(r)),'back'));
      back_y_ind = ~cellfun(@isempty,strfind(r.getStateFrame.coordinates(1:getNumDOF(r)),'back_bky'));
  
      integral_gains = zeros(getNumDOF(r),1);
      integral_clamps = zeros(getNumDOF(r),1);
      if options.controller_type == 1 || options.controller_type == 2 % use PID control
        integral_gains(arm_ind) = 1.0;
        integral_gains(back_ind) = 0.2;
        integral_clamps(arm_ind) = 0.3;
        integral_clamps(back_ind) = 0.2;
        integral_clamps(back_y_ind) = 0.1;
      end
      
      data = struct('qtraj',zeros(getNumDOF(r),1),...
                    'integral',zeros(getNumDOF(r),1),...
                    'integral_gains',integral_gains,...
                    'integral_clamps',integral_clamps,...
                    'enable_bdi_manip',true,...
                    'firstplan',true);
      ctrl_data = AtlasManipControllerData(data);

      options.use_error_integrator = true;
      qt = QTrajEvalBlock(r,ctrl_data,options);

      if options.controller_type == 1 % PID control
        
        % instantiate position ref publisher
        qref = PositionRefFeedthroughBlock(r);
        
        ins(1).system = 1;
        ins(1).input = 1;
        outs(1).system = 2;
        outs(1).output = 1;
        sys = mimoCascade(qt,qref,[],ins,outs);

      elseif options.controller_type == 2 % PID control w/pelvis adjustment
        
        % instantiate position ref publisher
        qref = PositionRefFeedthroughBlock(r);
        manip_cmd = BDIManipCommandBlock(r,ctrl_data);
        
        ins(1).system = 1;
        ins(1).input = 1;
        outs(1).system = 2;
        outs(1).output = 1;
        sys = mimoCascade(qt,manip_cmd,[],ins,outs);
        sys = mimoCascade(sys,qref,[],ins,outs);

      elseif options.controller_type == 3 % use PD + gravity compensation

        % cascade gravity compensation block
        gc = GravityCompensationBlock(r);
        ins(1).system = 1;
        ins(1).input = 1;
        outs(1).system = 1;
        outs(1).output = 1;
        outs(2).system = 2;
        outs(2).output = 1;
        conn(1).from_output = 2;
        conn(1).to_input = 1;        
        sys = mimoCascade(qt,gc,conn,ins,outs);
        clear ins outs conn;
        
        % cascade pos/torque ref feedthrough block
        options.open_loop_torque = false;
        q_tau_ref = PosTorqueRefFeedthroughBlock(r,options);
        ins(1).system = 1;
        ins(1).input = 1;
        outs(1).system = 2;
        outs(1).output = 1;
        sys = mimoCascade(sys,q_tau_ref,[],ins,outs);
        clear ins outs;
        
      elseif options.controller_type == 4 % use inverse dynamics

        % cascade eval block with signal duplicator
        dupl = SignalDuplicator(AtlasCoordinates(r),2);
        ins(1).system = 1;
        ins(1).input = 1;
        outs(1).system = 2;
        outs(1).output = 1;
        outs(2).system = 2;
        outs(2).output = 2;
        outs(3).system = 1;
        outs(3).output = 2;
        sys = mimoCascade(qt,dupl,[],ins,outs);
        clear ins outs;
        
        % cascade PD block
        options.Kp = 50.0*ones(getNumDOF(r),1);
        options.Kd = 12.0*ones(getNumDOF(r),1);
        options.use_qddtraj = true;
        options.use_ik = false;
        pd = IKPDBlock(r,ctrl_data,options);
        ins(1).system = 1;
        ins(1).input = 1;
        outs(1).system = 1;
        outs(1).output = 1;
        outs(2).system = 2;
        outs(2).output = 1;
        conn(1).from_output = 2;
        conn(1).to_input = 1;
        conn(2).from_output = 3;
        conn(2).to_input = 2;
        sys = mimoCascade(sys,pd,conn,ins,outs);
        clear ins outs conn;
        
        % cascade inverse dynamics block
        invdyn = InverseDynamicsBlock(r);
        ins(1).system = 1;
        ins(1).input = 1;
        ins(2).system = 2;
        ins(2).input = 2;      
        outs(1).system = 1;
        outs(1).output = 1;
        outs(2).system = 2;
        outs(2).output = 1;
        conn(1).from_output = 2;
        conn(1).to_input = 1;        
        sys = mimoCascade(sys,invdyn,conn,ins,outs);
        clear ins outs conn;
        
        % cascade pos/torque ref feedthrough block
        q_tau_ref = PosTorqueRefFeedthroughBlock(r);
        ins(1).system = 1;
        ins(1).input = 1;
        ins(2).system = 1;
        ins(2).input = 2;      
        outs(1).system = 2;
        outs(1).output = 1;
        sys = mimoCascade(sys,q_tau_ref,[],ins,outs);
        clear ins outs;
        
      end
      
      obj = obj@DRCController(name,sys,AtlasState(r));
 
      obj.robot = r;
      obj.controller_data = ctrl_data;
      obj.arm_joints = arm_ind;
      obj.back_joints = back_ind;
      
      % use saved nominal pose 
      d = load(strcat(getenv('DRC_PATH'),'/control/matlab/data/atlas_fp.mat'));
      q0 = d.xstar(1:getNumDOF(obj.robot));
      obj.controller_data.qtraj = q0((1+~obj.robot.floating*6):end);
      obj.controller_data.qddtraj = ConstantTrajectory(zeros(getNumDOF(r),1));
      
      obj = addLCMTransition(obj,'COMMITTED_ROBOT_PLAN',drc.robot_plan_t(),name); % for standing/reaching tasks
      obj = addLCMTransition(obj,'COMMITTED_PLAN_PAUSE',drc.plan_control_t(),name); % stop plan execution
      obj = addLCMTransition(obj,'ATLAS_COMMAND_UNSAFE',drc.atlas_command_t(),name); % set desired to previous 
      obj = addLCMTransition(obj,'ATLAS_BEHAVIOR_COMMAND',drc.atlas_behavior_command_t(),'init'); 
      obj = addLCMTransition(obj,'CALIBRATE_ARM_ENCODERS',drc.utime_t(),'init'); 
    end
    
    function msg = status_message(obj,t_sim,t_ctrl)
        msg = drc.controller_status_t();
        msg.utime = t_sim * 1000000;
        msg.state = msg.STANDING;
        msg.controller_utime = t_ctrl * 1000000;
        msg.V = 0;
        msg.Vdot = 0;
    end
    
    function obj = initialize(obj,data)
      
      if isfield(data,'COMMITTED_ROBOT_PLAN')
        % standing and reaching plan
        try
          msg = data.COMMITTED_ROBOT_PLAN;
          joint_names = obj.robot.getStateFrame.coordinates(1:getNumDOF(obj.robot));
          [xtraj,ts] = RobotPlanListener.decodeRobotPlan(msg,obj.robot.floating,joint_names); 
          
          if obj.controller_data.firstplan
            obj.controller_data.firstplan = false;
          else
            qtraj_prev = obj.controller_data.qtraj;
            q0=xtraj(1:getNumDOF(obj.robot),1);

            if isa(qtraj_prev,'PPTrajectory') 
              qprev_end = fasteval(qtraj_prev,data.t);
            else
              qprev_end = qtraj_prev;
            end

            % smooth transition from end of previous trajectory by adding
            % difference to integral terms
            integ = obj.controller_data.integral;
            torso = (obj.arm_joints | obj.back_joints);
            integ(torso) = integ(torso) + qprev_end(torso) - q0(torso);
            obj.controller_data.integral = integ;
          end
          
          qtraj = PPTrajectory(spline(ts,[zeros(getNumDOF(obj.robot),1), xtraj(1:getNumDOF(obj.robot),:), zeros(getNumDOF(obj.robot),1)]));

          obj.controller_data.qtraj = qtraj;
          obj.controller_data.qddtraj = fnder(qtraj,2);
        catch err
          disp(err);
          disp('error recieving plan, setting desired to current');

          x0 = data.AtlasState; % should always have an atlas state
          q0 = x0(1:getNumDOF(obj.robot));
          obj.controller_data.qtraj = q0((1+~obj.robot.floating*6):end);
          obj.controller_data.qddtraj = ConstantTrajectory(zeros(getNumDOF(obj.robot),1));
        end
        
      elseif isfield(data,'COMMITTED_PLAN_PAUSE')
        % set plan to current desired state
        qtraj = obj.controller_data.qtraj;

        if isa(qtraj,'PPTrajectory') 
          qtraj = fasteval(qtraj,data.t);
        end
        
        obj.controller_data.qtraj = qtraj;
        obj.controller_data.qddtraj = ConstantTrajectory(zeros(getNumDOF(obj.robot),1));

      elseif isfield(data,'ATLAS_COMMAND_UNSAFE')

        % set desired configuration to be the current state, set difference
        % between desired and current to integrators

        x0 = data.AtlasState; % should always have an atlas state
        q0 = x0(1:getNumDOF(obj.robot));
        obj.controller_data.qtraj = q0((1+~obj.robot.floating*6):end);
        obj.controller_data.qddtraj = ConstantTrajectory(zeros(getNumDOF(obj.robot),1));

        % get current desired pos on robot
        msg = data.ATLAS_COMMAND_UNSAFE;
        qdes = q0;
        qdes((1+obj.robot.floating*6):end) = msg.position(obj.robot.BDIToStateInd-obj.robot.floating*6); % sent back from driver in BDI ordering
     
        % note that if any(integ > max_integ), this will get thresholded in
        % the controller and produce a jump (this shouldn't happen
        % during normal operation)
        
        % set integral terms to be des-cur
        integ = obj.controller_data.integral;
        torso = (obj.arm_joints | obj.back_joints);
        integ(torso) = qdes(torso) - q0(torso);
        obj.controller_data.integral = integ;      

      end
      obj = setDuration(obj,inf,false); % set the controller timeout
    end
    
  end
end
