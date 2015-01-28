classdef LCMInputFromAtlasCommandBlock < MIMODrakeSystem
  
  properties
    lc;
    lcmonitor_cmd; %LCM monitors
    lcmonitor_neck;
    
    lcmtype_cmd_constructor;
    lcmtype_neck_constructor;
    coder_cmd;
    lcmtype_cmd;
    lcmtype_neck;
    
    lcmtype_cmd_coordinate_names;
    lcmtype_cmd_dim;
    timestamp_name;
    
    % Atlas and various controllers:
    r;
    r_control;
    pelvis_controller;
    fc;
    qt;
    pd_plus_qp_block;
    nq; % Atlas # of DOFS
    nu; % Atlas # of controllable DOFS
    joint_names;
    drake_to_atlas_joint_map;
    neck_in_i;
    neck_out_i;
    neck_desired_angle;
  end
  
  methods
    function obj = LCMInputFromAtlasCommandBlock(r, r_control, options)
      typecheck(r,'Atlas');
      % r_control is an atlas model with a state of just
      % atlas_state.
      if ~isempty(r_control)
        typecheck(r_control, 'Atlas');
      else
        r_control = r;
      end
      
      if nargin<2
        options = struct();
      end

      % Generate AtlasInput as out (we'll do translation manually)
      output_frame = drcFrames.AtlasInput(r);
      
      % We'll need atlas state as input
      input_frame = drcFrames.AtlasState(r_control);
      
      obj = obj@MIMODrakeSystem(0,0,input_frame,output_frame,true,false);
      obj = setInputFrame(obj,input_frame);
      obj = setOutputFrame(obj,output_frame);
      
      obj.r = r;
      obj.r_control = r_control;
      
      obj.lc = lcm.lcm.LCM.getSingleton();
      obj.lcmonitor_cmd = drake.util.MessageMonitor(drc.atlas_command_t,'utime');
      obj.lcmonitor_neck = drake.util.MessageMonitor(drc.neck_pitch_t,'utime');
      obj.lc.subscribe('ATLAS_COMMAND',obj.lcmonitor_cmd);
      obj.lc.subscribe('DESIRED_NECK_PITCH',obj.lcmonitor_neck);
      
      % Setup ATLAS_COMMAND_T lcm type
      lcmtype_cmd = drc.atlas_command_t;
      lcmtype_cmd = lcmtype_cmd.getClass();
      names={};
      f = lcmtype_cmd.getFields;
      for i=1:length(f)
        fname = char(f(i).getName());
        if strncmp(fname,'LCM_FINGERPRINT',15), continue; end
        if strcmp(fname,'utime')
          if ~strcmp(f(i).getGenericType.getName,'long')
            error('by convention, the timestamp field should be type int64_t');
          end
          obj.timestamp_name = 'utime';
          continue;
        end
        names{end+1}=fname;
      end
      obj.lcmtype_cmd = lcmtype_cmd;
      obj.lcmtype_cmd_coordinate_names = names;
      obj.lcmtype_cmd_dim = length(names);
      constructors_cmd = lcmtype_cmd.getConstructors();
      for i=1:length(constructors_cmd)
        f = constructors_cmd(i).getParameterTypes;
        if ~isempty(f) && strncmp('[B',char(f(1).getName),2)
          obj.lcmtype_cmd_constructor = constructors_cmd(i);
        end
      end
      
      % Setup NECK_PITCH_T lcm type
      lcmtype_neck = drc.neck_pitch_t;
      lcmtype_neck = lcmtype_neck.getClass();
      constructors_neck = lcmtype_neck.getConstructors();
      for i=1:length(constructors_neck)
        f = constructors_neck(i).getParameterTypes;
        if ~isempty(f) && strncmp('[B',char(f(1).getName),2)
          obj.lcmtype_neck_constructor = constructors_neck(i);
        end
      end
      
      % And the initial standing controller, until the planner comes
      % online
      obj = setup_init_planner(obj, options);
      
      % And the lcm coder
      obj.joint_names = obj.pd_plus_qp_block.getOutputFrame.getCoordinateNames;
      obj.joint_names = obj.joint_names(1:obj.nu);
      gains = struct();
      gains.k_qd_p = zeros(obj.nu,1);
      gains.k_q_i = zeros(obj.nu,1);
      gains.k_f_p = zeros(obj.nu,1);
      gains.ff_f_d = zeros(obj.nu,1);
      gains.ff_qd_d = zeros(obj.nu,1);
      gains.ff_const = zeros(obj.nu,1);
      [Kp,Kd] = getPDGains(r,'default');
      gains.k_q_p = diag(Kp);
      gains.ff_qd = diag(Kd);
      obj.coder_cmd = drc.control.AtlasCommandCoder(obj.joint_names,r.atlas_version,gains.k_q_p*0,gains.k_q_i*0,...
        gains.k_qd_p,gains.k_f_p*0,gains.ff_qd,gains.ff_qd_d,gains.ff_f_d*0,gains.ff_const*0);
      
      % And compute for ourselves the drake_to_atlas_joint_map
      obj.drake_to_atlas_joint_map = zeros(length(obj.joint_names), 1);
      dummyInput = drcFrames.AtlasInput(obj.r);
      for i=1:length(obj.joint_names)
        in_joint_name_i = obj.joint_names(i, :);
        obj.drake_to_atlas_joint_map(i) = dummyInput.findCoordinateIndex(strtrim(in_joint_name_i));
      end

      % Set up neck joint indices
      dummyInput = drcFrames.AtlasInput(obj.r_control);
      dummyState = drcFrames.AtlasState(obj.r_control);
      obj.neck_in_i = dummyState.findCoordinateIndex('neck_ay');
      obj.neck_in_i = obj.neck_in_i(1);
      obj.neck_out_i = dummyInput.findCoordinateIndex('neck_ay');
      obj.neck_desired_angle = SharedDataHandle(0);
      
    end
    
    function obj=setup_init_planner(obj, options)
      r = obj.r_control;
      S = load(r.fixed_point_file);
      x_init = Point(r.getStateFrame(),getInitialState(r));
      xstar = Point(r.getStateFrame(),S.xstar);
      xstar.base_x = x_init.base_x;
      xstar.base_y = x_init.base_y;
      xstar.base_yaw = x_init.base_yaw;
      
      x0 = double(xstar);
      obj.nq = getNumPositions(r);
      obj.nu = getNumInputs(r);
      q0 = x0(1:obj.nq);
      kinsol = doKinematics(r,q0);
      
      com = getCOM(r,kinsol);
      
      % build TI-ZMP controller
      footidx = [findLinkId(r,'l_foot'), findLinkId(r,'r_foot')];
      foot_pos = terrainContactPositions(r,kinsol,footidx);
      comgoal = mean([mean(foot_pos(1:2,1:4)');mean(foot_pos(1:2,5:8)')])';
      limp = LinearInvertedPendulum(com(3));
      [~,V] = lqr(limp,comgoal);
      
      foot_support = RigidBodySupportState(r,find(~cellfun(@isempty,strfind(r.getLinkNames(),'foot'))));
      
      pelvis_idx = findLinkId(r,'pelvis');
      
      link_constraints(1).link_ndx = pelvis_idx;
      link_constraints(1).pt = [0;0;0];
      link_constraints(1).traj = ConstantTrajectory(forwardKin(r,kinsol,pelvis_idx,[0;0;0],1));
      link_constraints(2).link_ndx = footidx(1);
      link_constraints(2).pt = [0;0;0];
      link_constraints(2).traj = ConstantTrajectory(forwardKin(r,kinsol,footidx(1),[0;0;0],1));
      link_constraints(3).link_ndx = footidx(2);
      link_constraints(3).pt = [0;0;0];
      link_constraints(3).traj = ConstantTrajectory(forwardKin(r,kinsol,footidx(2),[0;0;0],1));
      
      ctrl_data = QPControllerData(false,struct(...
        'acceleration_input_frame',drcFrames.AtlasCoordinates(r),...
        'D',-com(3)/9.81*eye(2),...
        'Qy',eye(2),...
        'S',V.S,...
        's1',zeros(4,1),...
        's2',0,...
        'x0',[comgoal;0;0],...
        'u0',zeros(2,1),...
        'y0',comgoal,...
        'qtraj',x0(1:obj.nq),...
        'support_times',0,...
        'supports',foot_support,...
        'link_constraints',link_constraints,...
        'mu',1.0,...
        'ignore_terrain',false,...
        'plan_shift',zeros(3,1),...
        'constrained_dofs',[findPositionIndices(r,'arm');findPositionIndices(r,'back');findPositionIndices(r,'neck')]));
      
      % instantiate QP controller
      options.slack_limit = 30.0;
      nq = getNumPositions(r);
      options.w_qdd = 0.001*ones(nq,1);
      options.w_grf = 0;
      options.w_slack = 0.001;
      options.Kp_pelvis = [150; 150; 150; 200; 200; 200];
      options.pelvis_damping_ratio = 0.6;
      options.Kp_q = 150.0*ones(r.getNumPositions(),1);
      options.q_damping_ratio = 0.6;
      options.W_kdot = zeros(3);
      
      % construct QP controller and related control blocks
      [qp,~,~,pelvis_controller,pd,options] = constructQPBalancingController(r,ctrl_data,options);
      obj.pelvis_controller = pelvis_controller;
      options.use_lcm=false;
      options.contact_threshold = 0.002;
      obj.fc = atlasControllers.FootContactBlock(r,ctrl_data,options);
      obj.qt = atlasControllers.QTrajEvalBlock(r,ctrl_data,options);
      
      ins(1).system = 1;
      ins(1).input = 1;
      ins(2).system = 1;
      ins(2).input = 2;
      ins(3).system = 2;
      ins(3).input = 1;
      ins(4).system = 2;
      ins(4).input = 3;
      ins(5).system = 2;
      ins(5).input = 4;
      outs(1).system = 2;
      outs(1).output = 1;
      outs(2).system = 2;
      outs(2).output = 2;
      pd = pd.setOutputFrame(drcFrames.AtlasCoordinates(r));
      obj.pd_plus_qp_block = mimoCascade(pd,qp,[],ins,outs);
      clear ins;
      %fprintf('Current time: xxx.xxx');
    end
    
    function x=decode_cmd(obj, data)
      msg = obj.lcmtype_cmd_constructor.newInstance(data);
      x=cell(obj.dim, 1);
      for i=1:obj.dim
        eval(['x{',num2str(i),'} = msg.',CoordinateFrame.stripSpecialChars(obj.lcmtype_cmd_coordinate_names{i}),';']);
        % fix string type
        if (isa(x{i}, 'java.lang.String[]'))
          x{i} = char(x{i});
        end
      end
      eval(['t = msg.', obj.timestamp_name, '/1000;']);
    end
    
    function varargout=mimoOutput(obj,t,~,atlas_state)
      
      % What needs to go out:
      %atlas_state_names = obj.getInputFrame.getCoordinateNames();
      efforts = zeros(obj.getNumOutputs, 1);
      
      % check for new neck cmd
      data_neck = obj.lcmonitor_neck.getMessage();
      if (~isempty(data_neck))
       neck_cmd = obj.lcmtype_neck_constructor.newInstance(data_neck);
       obj.neck_desired_angle.setData(neck_cmd.pitch);
      end
      
      % see if we have a new message (new command state)
      data = obj.lcmonitor_cmd.getMessage();
        
      % If we haven't received a command make our own
      if (isempty(data))
        % foot contact
        [phiC,~,~,~,~,idxA,idxB,~,~,~] = obj.r_control.getManipulator().contactConstraints(atlas_state(1:obj.r_control.getNumPositions()),false);
        within_thresh = phiC < 0.002;
        contact_pairs = [idxA(within_thresh) idxB(within_thresh)];
        fc = [any(any(contact_pairs == obj.r_control.findLinkId('l_foot')));
              any(any(contact_pairs == obj.r_control.findLinkId('r_foot')))];
            
        % qtraj eval
        q_des_and_x = output(obj.qt,t,[],atlas_state);
        q_des = q_des_and_x(1:obj.nq);
        % IK/QP
        pelvis_ddot = output(obj.pelvis_controller,t,[],atlas_state);
        u_and_qdd = output(obj.pd_plus_qp_block,t,[],[q_des; atlas_state; atlas_state; fc; pelvis_ddot]);
        u=u_and_qdd(1:obj.nu);
        for i=1:obj.nu
          efforts(obj.drake_to_atlas_joint_map(i)) = u(i);
        end
      else
        cmd = obj.coder_cmd.decode(data);
        efforts = cmd.val(obj.nu*2+1:end);
      end
      
      % And set neck pitch via simple PD controller
      
      error =  obj.neck_desired_angle.getData - atlas_state(obj.neck_in_i);
      vel = atlas_state(length(atlas_state)/2 + obj.neck_in_i);
      efforts(obj.neck_out_i) = 50*error - vel;
      varargout = {efforts(1:obj.nu)};
      %fprintf('\b\b\b\b\b\b\b%7.3f', t);
      
    end
  end
  
end
