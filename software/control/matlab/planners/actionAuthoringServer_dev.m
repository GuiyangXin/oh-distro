function actionAuthoringServer(IK,action_options)
% IK = 1 means we only require an IK solution
% IK = 2 meas that the we also do ZMP planning
if(nargin<1)
    action_options.IK = true;
    action_options.ZMP = false;
    action_options.QS = false;
elseif(IK == 1)
    action_options.IK = true;
    action_options.ZMP = false;
    action_options.QS = false;
elseif(IK == 2)
    action_options.IK = false;
    action_options.ZMP = true;
    action_options.QS = false;
elseif(IK == 3)
    action_options.IK = false;
    action_options.ZMP = false;
    action_options.QS = true;
end

% Parse options structure
if(~isfield(action_options,'drake_vis')) action_options.drake_vis = false; end
if(~isfield(action_options,'use_mex')) action_options.use_mex = false; end
if(~isfield(action_options,'debug')) action_options.debug = false; end
if(~isfield(action_options,'verbose')) action_options.verbose = false; end
if(~isfield(action_options,'run_once')) action_options.run_once = false; end
if(~isfield(action_options,'ignore_q0')) action_options.ignore_q0 = false; end
if(~isfield(action_options,'generate_implicit_constraints_from_q0')) action_options.generate_implicit_constraints_from_q0 = true; end
if(~isfield(action_options,'use_inverseKinSequence')) action_options.use_inverseKinSequence = false; end
if(~isfield(action_options,'channel_in')) 
  if(action_options.IK)
    action_options.channel_in = 'REQUEST_IK_SOLUTION_AT_TIME_FOR_ACTION_SEQUENCE'; 
  else
    action_options.channel_in = 'REQUEST_MOTION_PLAN_FOR_ACTION_SEQUENCE'; 
  end
end

% listens for drc_action_sequence_t messages and, upon receipt, computes the
% IK and publishes the robot_state_t

lc = lcm.lcm.LCM.getSingleton(); %('udpm://239.255.76.67:7667?ttl=1');

% construct lcm input monitor
monitor = drake.util.MessageMonitor(drc.action_sequence_t(),'utime');
if (IK==1)
  lc.subscribe(action_options.channel_in,monitor);
elseif (IK==2)
  lc.subscribe(action_options.channel_in,monitor);
elseif (IK==3)
  lc.subscribe(action_options.channel_in,monitor);
end

% construct lcm state publisher
% todo: should really load model name from lcm

s=warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits');
warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints');
r = RigidBodyManipulator('');
%r = r.addRobotFromURDF('../../models/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf', [],[],struct('floating',true));
r = r.addRobotFromURDF('../../models/mit_gazebo_models/mit_robot_drake/model_simple_visuals_point_hands.urdf', [],[],struct('floating',true));
r_Atlas = Atlas('../../models/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf',struct('floating',true));
r = r.addRobotFromURDF('../../models/mit_gazebo_objects/mit_vehicle/model_drake.urdf',[0;0;0],[0;0;0]);
warning(s);

nq = r.getNumDOF();
% load the "zero position"
load(strcat(getenv('DRC_PATH'),'/control/matlab/data/atlas_fp.mat'));
q_standing = xstar(1:nq);
q = q_standing;
options.q_traj_nom = ConstantTrajectory(q);

%r.getStateFrame.subscribe('EST_ROBOT_STATE');

joint_names = r.getStateFrame.coordinates(1:getNumDOF(r));
robot_state_coder = LCMCoordinateFrame('AtlasState',JLCMCoder(drc.control.RobotStateConstraintCheckedCoder( joint_names)),'x');
%robot_plan_publisher =  drc.control.RobotPlanConstraintCheckedPublisher(joint_names,true, ...  
  %'RESPONSE_MOTION_PLAN_FOR_ACTION_SEQUENCE');
robot_plan_publisher =  drc.control.RobotPlanPublisher(joint_names,true, ...  
  'RESPONSE_MOTION_PLAN_FOR_ACTION_SEQUENCE');
robot_plan_publisher_viewer =  drc.control.WalkingPlanPublisher('QUASISTATIC_ROBOT_PLAN');
%%


% setup IK prefs
cost = Point(r.getStateFrame,1);
cost.base_x = 0;
cost.base_y = 0;
cost.base_z = 0;
arm_cost = 1e2;
cost.l_arm_usy = arm_cost;
cost.l_arm_shx = arm_cost;
cost.l_arm_ely = arm_cost;
cost.l_arm_elx = arm_cost;
cost.l_arm_uwy = arm_cost;
cost.r_arm_usy = arm_cost;
cost.r_arm_shx = arm_cost;
cost.r_arm_ely = arm_cost;
cost.r_arm_elx = arm_cost;
cost.r_arm_uwy = arm_cost;
cost.base_roll = 0;
cost.base_pitch =0;
cost.base_yaw = 0;
cost.back_bky = 100;
cost.back_bkx = 100;
cost = double(cost);
options = struct();
options.q_nom = q_standing;
options.Q = diag(cost(1:r.getNumDOF));
[jointLimitMin, jointLimitMax] = r.getJointLimits();
joint_names = r.getStateFrame.coordinates(1:r.getNumDOF());
knee_ind = find(~cellfun(@isempty,strfind(joint_names,'kny')));
elbow_ind = find(~cellfun(@isempty,strfind(joint_names,'elx')));
back_ind = find(~cellfun(@isempty,strfind(joint_names,'ubx')) | ~cellfun(@isempty,strfind(joint_names,'mby')));
hip_ind = find( ...
                ~cellfun(@isempty,strfind(joint_names,'l_leg_hpz')) ...
              );
                %| ~cellfun(@isempty,strfind(joint_names,'mhx')) ...
                %| ~cellfun(@isempty,strfind(joint_names,'uay')) ...

jointLimitShrink = ones(size(jointLimitMin));
% jointLimitShrink(back_ind) = 0.4;
%jointLimitShrink(hip_ind) = 0.6;
jointLimitHalfLength = jointLimitShrink.*(jointLimitMax - jointLimitMin)/2;
jointLimitMid = (jointLimitMax + jointLimitMin)/2;

options.jointLimitMin = jointLimitMid - jointLimitHalfLength;
options.jointLimitMin(isnan(options.jointLimitMin)) = -Inf;
options.jointLimitMax = jointLimitMid + jointLimitHalfLength;
options.jointLimitMax(isnan(options.jointLimitMax)) = Inf;


% options.jointLimitMin(knee_ind) = 0.6;
% options.jointLimitMin(hip_ind) = 0.0;

options.use_mex = action_options.use_mex;

contact_tol = 1e-4;

if(action_options.drake_vis)
  v = r.constructVisualizer();
  v.draw(0,q);
end

clear jointLimitShrink jointLimitHalfLength jointLimitHalfLength jointLimitMid jointLimitMin jointLimitMax knee_ind elbow_ind back_ind hip_ind joint_names cost arm_cost IK ustar zstar xstar;

timeout=10;
display('Listening ...');
while (1)
  warning on
  data = getNextMessage(monitor,timeout);
  if ~isempty(data)
    msg = drc.action_sequence_t(data);
    ik_time = msg.ik_time;
    action_sequence = ActionSequence();
    q_bk = q;
    % Get initial conditions from msg.q0
    msg.q0.robot_name = 'atlas'; % To match robot_state_coder.lcmcoder
    x0 = robot_state_coder.lcmcoder.jcoder.decode(msg.q0).val;
    if any(x0 > eps)
      q = x0(1:getNumDOF(r));
      fprintfVerb(action_options,'Taking initial state from action sequence message\n');
    else
      fprintfVerb(action_options,'Using default standing pose for intial state\n');
    end
    try
      for i=1:msg.num_contact_goals
        goal = msg.contact_goals(i);
        if ~action_options.generate_implicit_constraints_from_q0 && roundn(goal.lower_bound_completion_time,-6) == 0.1
          goal.lower_bound_completion_time = 0;
        end
        kc = getConstraintFromGoal(r,goal);
        action_sequence = action_sequence.addKinematicConstraint(kc);
        fprintfVerb(action_options,'Added constraint %d: %s\n',numel(action_sequence.kincons),action_sequence.kincons{i}.name);
      end
      n_kincons = length(action_sequence.kincons);

      for i=1:length(action_sequence.kincons)
%         if(action_sequence.kincons{i}.tspan(1) == action_sequence.tspan(1))
%           contact_state0 = action_sequence.kincons{i}.contact_state0 ;
%           contact_statei = action_sequence.kincons{i}.contact_statei ;
%           for j = 1:length(contact_state0)
%             ind_make = contact_state0{j}==ActionKinematicConstraint.MAKE_CONTACT;
%             contact_state0{j}(ind_make) = contact_statei{j}(ind_make);
%           end
%           action_sequence.kincons{i}.contact_state0 = contact_state0;
%         end
%         if(action_sequence.kincons{i}.tspan(2) == action_sequence.tspan(2))
%           contact_statef = action_sequence.kincons{i}.contact_statef ;
%           contact_statei = action_sequence.kincons{i}.contact_statei ;
%           for j = 1:length(contact_statef)
%             ind_break = contact_statef{j}==ActionKinematicConstraint.BREAK_CONTACT;
%             contact_statef{j}(ind_break) = contact_statei{j}(ind_break);
%           end
%           action_sequence.kincons{i}.contact_statef = contact_statef;
%         end
      end
      if action_options.generate_implicit_constraints_from_q0
        action_sequence = generateImplicitConstraints(action_sequence,r,q,action_options);
        n_kincons_new = length(action_sequence.kincons);
        if n_kincons_new > n_kincons
          for i = (n_kincons+1):n_kincons_new
            fprintfVerb(action_options,'Added constraint %d: %s\n',i,action_sequence.kincons{i}.name);
          end
          n_kincons = n_kincons_new;
        end
      end
      action_sequence = action_sequence.addStaticContactConstraint(r,q,action_sequence.key_time_samples(1));
      n_kincons_new = length(action_sequence.kincons);
      if n_kincons_new > n_kincons
        for i = (n_kincons+1):n_kincons_new
          fprintfVerb(action_options,'Added constraint %d: %s\n',i,action_sequence.kincons{i}.name);
        end
        n_kincons = n_kincons_new;
      end


      % Above ground constraints
      %tspan = action_sequence.tspan;
      %for body_ind = 1:length(r.body)
        %body_contact_pts = r.body(body_ind).getContactPoints();
        %if(~isempty(body_contact_pts))
          %above_ground_constraint = ActionKinematicConstraint.groundConstraint(r,body_ind,body_contact_pts,tspan,[r.body(body_ind).linkname,'_above_ground_from_',num2str(tspan(1)),'_to_',num2str(tspan(2))]);
          %action_sequence = action_sequence.addKinematicConstraint(above_ground_constraint);
        %end 
      %end
      
      % Solve the IK sequentially in time for each key time
      %if action_sequence.key_time_samples(1) > eps
        %action_sequence.tspan(1) = 0;
        %action_sequence.key_time_samples = [0, action_sequence.key_time_samples];
      %end
      num_key_time_samples = length(action_sequence.key_time_samples);
      com_key_time_samples = zeros(3,num_key_time_samples);
      q_key_time_samples = zeros(nq,num_key_time_samples);
      q_key_time_samples(:,1) = q;
      kinsol = doKinematics(r,q_key_time_samples(:,1));
      com_key_time_samples(:,1) = getCOM(r,kinsol);
      options.quasiStaticFlag = true;
      options.shrinkFactor = 0.5;
      options.q_nom = q_standing;
      if(action_options.ignore_q0)
        ind_first_time= 1;
      else
        ind_first_time = 2;
      end

      key_time_IK_failed = false;
      
      support_times = action_sequence.key_time_samples;
      support_body_ind = zeros(size(support_times));
      support_body_ind = [];
      contact_surface_ind = [];
      support_states = {};
      for i = 1:num_key_time_samples 
        if(i==num_key_time_samples)
          for j=1:length(action_sequence.kincons)
              %         if(action_sequence.kincons{i}.tspan(1) == action_sequence.tspan(1))
              %           contact_state0 = action_sequence.kincons{i}.contact_state0 ;
              %           contact_statei = action_sequence.kincons{i}.contact_statei ;
              %           for j = 1:length(contact_state0)
              %             ind_make = contact_state0{j}==ActionKinematicConstraint.MAKE_CONTACT;
              %             contact_state0{j}(ind_make) = contact_statei{j}(ind_make);
              %           end
              %           action_sequence.kincons{i}.contact_state0 = contact_state0;
              %         end
              if(action_sequence.kincons{j}.tspan(2) == action_sequence.tspan(2))
                  contact_statef = action_sequence.kincons{j}.contact_statef ;
                  contact_statei = action_sequence.kincons{j}.contact_statei ;
                  for k = 1:length(contact_statef)
                      ind_break = contact_statef{k}==ActionKinematicConstraint.BREAK_CONTACT;
                      contact_statef{k}(ind_break) = contact_statei{k}(ind_break);
                  end
                  action_sequence.kincons{j}.contact_statef = contact_statef;
              end
          end
        end
        ikargs = action_sequence.getIKArguments(action_sequence.key_time_samples(i));
        if i >= ind_first_time
          if(isempty(ikargs))
            q_key_time_samples(:,i) = ...
              options.q_traj_nom.eval(action_sequence.key_time_samples(i));
          else
            if true || i==1
              q0 = q;
              q0(7:end) = q_standing(7:end);
            else
              q0 = q_key_time_samples(:,i-1);
            end
            [q_key_time_samples(:,i),info] = inverseKin(r,q0,ikargs{:},options);
            if(info>10)
              warning(['IK at time ',num2str(action_sequence.key_time_samples(i)),' is not successful']);
              key_time_IK_failed = true;
              break;
            else
              fprintf('IK at time %5.3f successful\n',action_sequence.key_time_samples(i));
            end
            if(i<num_key_time_samples)
              action_sequence = action_sequence.addStaticContactConstraint(r,q_key_time_samples(:,i),action_sequence.key_time_samples(i));
              n_kincons_new = length(action_sequence.kincons);
              if n_kincons_new > n_kincons
                for j = (n_kincons+1):n_kincons_new
                  fprintfVerb(action_options,'Added constraint %d: %s\n',j,action_sequence.kincons{j}.name);
                end
                n_kincons = n_kincons_new;
              end
            end
          end
        end
        support_body_ind = [];
        surface_body_ind = [];
        support_point_ind = {};
        support_point_ind_unique = {};
        for j = 1:length(action_sequence.kincons)
          if action_sequence.key_time_samples(i) >= action_sequence.kincons{j}.tspan(1) && ...
              action_sequence.key_time_samples(i) <= action_sequence.kincons{j}.tspan(2) && ...
              any(any(cell2mat(action_sequence.kincons{j}.getContactState(action_sequence.key_time_samples(i))') == ActionKinematicConstraint.STATIC_PLANAR_CONTACT,1) | ...
              any(cell2mat(action_sequence.kincons{j}.getContactState(action_sequence.key_time_samples(i))') == ActionKinematicConstraint.STATIC_GRIP_CONTACT,1)|...
              any(cell2mat(action_sequence.kincons{j}.getContactState(action_sequence.key_time_samples(i))') == ActionKinematicConstraint.MAKE_CONTACT,1)|...
              any(cell2mat(action_sequence.kincons{j}.getContactState(action_sequence.key_time_samples(i))') == ActionKinematicConstraint.BREAK_CONTACT,1))
            support_body_ind = [support_body_ind, action_sequence.kincons{j}.body_ind];
            B = r.body(support_body_ind(end));
            body_contact_points = r_Atlas.getBodyContacts(support_body_ind(end));
            [~,support_point_ind_j] = ismember(action_sequence.kincons{j}.body_pts',body_contact_points','rows');
            support_point_ind = [support_point_ind, support_point_ind_j];
            surface_body_ind = [surface_body_ind, -1];
          end
        end
        [support_body_ind_unique,ia,ic] = unique(support_body_ind,'stable');
        for j = 1:length(support_body_ind_unique)
          support_point_ind_unique{j} = unique(vertcat(support_point_ind{ic==j}));
        end
        surface_body_ind = surface_body_ind(ia);
        support_states = [support_states; {RigidBodySupportState(r_Atlas,support_body_ind_unique,support_point_ind_unique,surface_body_ind)}];
        
        
        kinsol = doKinematics(r,q_key_time_samples(:,i));
        com_key_time_samples(:,i) = getCOM(r,kinsol);
      end
      publish(robot_plan_publisher, action_sequence.key_time_samples(1:i), ...
        [q_key_time_samples(:,1:i); 0*q_key_time_samples(:,1:i)]);
      if(key_time_IK_failed)
        error('IK at key times was not successful! Publishing key time results.');
      else
        key = input('Enter ''y''to refine trajectory. Press any other key to listen for new action sequence.');
        if ~strcmp(key,'y')
          continue;
        end
      end
      if(action_options.drake_vis)
        v.draw(0,q_key_time_samples(:,1));
      end

      if(action_options.QS)
        warning on
        %dt = 0.5;
        q0 = q;
        %if action_sequence.key_time_samples(1) > 0
          %action_sequence.tspan(1) = 0;
          %action_sequence.key_time_samples = [0, action_sequence.key_time_samples];
          %q_qs_plan = [q0, q_key_time_samples];
        %else
          %t_qs_breaks = action_sequence.key_time_samples;
          %q_qs_plan = q_key_time_samples;
          %q_qs_plan(:,1) = q0;
        %end
        t_qs_breaks = action_sequence.key_time_samples;
        q_qs_plan = q_key_time_samples;
        %q0 = q_qs_plan(:,1);
        if(action_options.use_inverseKinSequence)
          qdot0 = zeros(size(q));
          options.qtraj0 = PPTrajectory(spline(t_qs_breaks,q_qs_plan));
          %options.qtraj0 = PPTrajectory(foh(t_qs_breaks,q_qs_plan));
          options.quasiStaticFlag = true;
          %options.Qv = eye(length(q0));
          options.Qa = 1e0*eye(length(q0));
          %window_size = ceil((action_sequence.tspan(end)-action_sequence.tspan(1))/dt);
          %%t_qs_breaks = action_sequence.tspan(1)+dt*(0:window_size-1);
          %t_s_breaks = action_sequence.tspan(1)+dt*(0:window_size);
          %t_qs_breaks(end) = action_sequence.tspan(end);
          %options.nSample = length(t_qs_breaks)-1;
          options.nSample = 1;
          [t_qs_breaks, q_qs_plan, qdot_qs_plan, qddot_qs_plan, inverse_kin_sequence_info] = inverseKinSequence(r, q0, qdot0, action_sequence,options);
          %inverse_kin_sequence_info
          if(inverse_kin_sequence_info>10)
            error('IK sequence was not successful! ');
          else
            fprintf('IK sequence successful!\n',t_qs_breaks(i));
          end
        end
        com_qs_plan = zeros(3,numel(t_qs_breaks));
        qdot_qs_plan = zeros(size(q_qs_plan));
        for i = 1:numel(t_qs_breaks)
          kinsol = doKinematics(r,q_qs_plan(:,i));
          [com_qs_plan(:,i),J] = getCOM(r,kinsol);
          comdot_qs_plan(:,i) = J*qdot_qs_plan(:,i);
          if(action_options.verbose)
            fprintf('COM found time at %5.3f successful\n', ...
              t_qs_breaks(i));
          end
        end
        q_qs_traj = PPTrajectory(pchipDeriv(t_qs_breaks,q_qs_plan,qdot_qs_plan));
        com_qs_traj = PPTrajectory(pchipDeriv(t_qs_breaks,com_qs_plan,zeros(size(com_qs_plan))));

        % Refine
        if(action_options.verbose)
          disp('Refining trajectory')
        end
        dt = 0.05;
        com_kc = ActionKinematicConstraint(r,0,[0;0;0], ...
          com_qs_traj,action_sequence.tspan);
        action_sequence = action_sequence.addKinematicConstraint(com_kc);
        window_size = ceil((action_sequence.tspan(end)-action_sequence.tspan(1))/dt);
        t_qs_breaks = action_sequence.tspan(1)+dt*(0:window_size-1);
        %t_qs_breaks = action_sequence.tspan(1)+dt*(0:window_size);
        t_qs_breaks(end) = action_sequence.tspan(end);
        q_qs_plan = zeros(size(q_qs_plan,1),numel(t_qs_breaks));
        %options.Q = 1e0*eye(length(q0));
        options.quasiStaticFlag = true;
        options = rmfield(options,'q_nom');
        foot_support_qs = zeros(length(r.body),numel(t_qs_breaks));
        
        % Compute support at t0
        for i = 1:numel(t_qs_breaks)
          ikargs = action_sequence.getIKArguments(t_qs_breaks(i));
          if(isempty(ikargs))
            q_qs_plan(:,i) = q_qs_traj.eval(t_qs_breaks(i));
          elseif i >= ind_first_time
            [q_qs_plan(:,i),info] = ...
              inverseKin(r,q_qs_traj.eval(t_qs_breaks(i)),ikargs{:},options);
            if(info>10)
              warning(['IK at time ',num2str(t_qs_breaks(i)),' is not successful']);
            elseif(mod(t_qs_breaks(i),1) < dt-eps)
              fprintf('IK successful at time %5.3f \n',t_qs_breaks(i));
            else
              if(action_options.verbose)
                fprintf('IK at time %5.3f successful\n', ...
                  t_qs_breaks(i));
              end
            end
          else
              q_qs_plan(:,i) = q0;
          end
          j = 1;
          n = 1;
          while j<length(ikargs)
            if(isa(ikargs{j},'RigidBody'))
              ikargs{j} = find(r.body==ikargs{j},1);
            end
            body_ind{i}(n) = ikargs{j};
            if(body_ind{i}(n) == 0)
              j = j+2;
            else
              body_pos{i}{n} = ikargs{j+1};
              if(ischar(body_pos{i}{n})||numel(body_pos{i}{n})==1)
                body_pos{i}{n} = getContactPoints(r.body(body_ind{i}(n)),body_pos{i}{n});
              end
              if(i == 1)
                support_polygon_flags{i}{n} = ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.STATIC_PLANAR_CONTACT,1) | ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.STATIC_GRIP_CONTACT,1) | ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.MAKE_CONTACT,1);
              elseif(i == numel(t_qs_breaks))
                support_polygon_flags{i}{n} = ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.STATIC_PLANAR_CONTACT,1) | ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.STATIC_GRIP_CONTACT,1) | ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.BREAK_CONTACT,1);
              else
                support_polygon_flags{i}{n} = ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.STATIC_PLANAR_CONTACT,1) | ...
                  any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.STATIC_GRIP_CONTACT,1)|...
                  (any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.MAKE_CONTACT,1)&...
                   any(cell2mat(ikargs{j+2}.contact_state') == ActionKinematicConstraint.BREAK_CONTACT,1));
              end
              contact_state{i}{n} = ikargs{j+2}.contact_state;
              j = j+3;
              [rows,mi] = size(body_pos{i}{n});
              if(rows~=3) error('bodypos must be 3xmi');end
              num_sequence_support_vertices{i}(n) = sum(support_polygon_flags{i}{n});
              foot_support_qs(body_ind{i}(n),i) = any(support_polygon_flags{i}{n});
            end
            n = n+1;
          end
          num_sample_support_vertices(i) = sum(num_sequence_support_vertices{i});
          kinsol = doKinematics(r,q_qs_plan(:,i));
          total_body_support_vert = 0;
          com_qs_plan(:,i) = getCOM(r,kinsol);
          support_vert_pos{i} = zeros(2,num_sample_support_vertices(i));
          for j = 1:length(body_ind{i})
            if(body_ind{i}(j) ~= 0)
              [x,J] = forwardKin(r,kinsol,body_ind{i}(j),body_pos{i}{j},0); 
              support_vert_pos{i}(:,total_body_support_vert+(1:num_sequence_support_vertices{i}(j)))...
                = x(1:2,support_polygon_flags{i}{j});
              total_body_support_vert = total_body_support_vert+num_sequence_support_vertices{i}(j);
            end
          end
        end

        %options.q_traj_nom = PPTrajectory(spline(t_qs_breaks,q_qs_plan));

        % publish t_breaks, q_qs_plan with RobotPlanPublisher.java
        constraints_satisfied = ones(max(1,msg.num_contact_goals), ...
          size(q_qs_plan,2));

        %publish(robot_plan_publisher, t_qs_breaks, ...
        %[q_qs_plan; 0*q_qs_plan], ...
        %constraints_satisfied);
        publish(robot_plan_publisher, t_qs_breaks, ...
          [q_qs_plan; 0*q_qs_plan]);
        
        key = input('Enter ''y''to send the plan to the robot. Press any other key to listen for new action sequence.');
        if ~strcmp(key,'y')
          continue;
        end
        % Publish plan to viewer
        Q = 1*eye(4);
        R = 0.001*eye(2);
        comgoal = com_qs_plan(1:2,end);
        ltisys = LinearSystem([zeros(2),eye(2); zeros(2,4)],[zeros(2); eye(2)],[],[],[],[]);
        [~,V] = tilqr(ltisys,Point(getStateFrame(ltisys),[comgoal;0*comgoal]),Point(getInputFrame(ltisys)),Q,R);

        % compute TVLQR
        options.tspan = linspace(com_qs_traj.tspan(1),com_qs_traj.tspan(2),10);
        options.sqrtmethod = false;
        x0traj = setOutputFrame([com_qs_traj(1:2);0;0],ltisys.getStateFrame);
        u0traj = setOutputFrame(ConstantTrajectory([0;0]),ltisys.getInputFrame);
        S = warning('off','Drake:TVLQR:NegativeS');  % i expect to have some zero eigenvalues, which numerically fluctuate below 0
        warning(S);
        [~,V] = tvlqr(ltisys,x0traj,u0traj,Q,R,V,options);

        mu=0.5;
        data = struct('S',V.S,'s1',V.s1,'s2',V.s2,...
          'support_times',support_times,'supports',{support_states},'comtraj',com_qs_traj,'qtraj',q_qs_traj,'mu',mu,...
          'link_constraints',[],'zmptraj',[],'qnom',[]);

        robot_plan_publisher_viewer.publish(0,data);

        % Drake gui playback
        if(action_options.drake_vis)
          xtraj = PPTrajectory(pchip(t_qs_breaks,[q_qs_plan;0*q_qs_plan]));
          xtraj = xtraj.setOutputFrame(r.getStateFrame());
          v.playback(xtraj,struct('slider',true));
        end
        if(action_options.debug)
          key = input('Press ''s'' to save and continue, or any other key to continue without saving...','s');
          if strcmp(key,'s')
            % Shift trajectories to be in the body frame of the link specified by
            % action_options.ref_link_str, if present.
            if(isfield(action_options,'ref_link_str'))
              typecheck(action_options.ref_link_str,'char');
              ref_link = findLink(r,action_options.ref_link_str);
              pelvis = r.findLink('pelvis');
              for i = 1:length(t_qs_breaks)
                kinsol = doKinematics(r,q_qs_plan(:,i),false,false);
                com_i = getCOM(r,kinsol);
                if i == 1
                  wTf = ref_link.T;
                  fTw = [ [wTf(1:3,1:3)'; zeros(1,3)], [-wTf(1:3,1:3)'*wTf(1:3,4); 1] ];
                end
                com_qs_plan(:,i) = homogTransMult(fTw,com_i);
                wTr_i = pelvis.T;
                fTr_i = fTw*wTr_i;
                q_qs_plan(1:6,i) = [fTr_i(1:3,4); rotmat2rpy(fTr_i(1:3,1:3))];
              end
              ref_link_str = action_options.ref_link_str;
              uisave({'t_qs_breaks','q_qs_plan','com_qs_plan','support_vert_pos', ...
                'foot_support_qs','ref_link_str'},'data/aa_plan.mat');
            else
            uisave({'t_qs_breaks','q_qs_plan','com_qs_plan','support_vert_pos', ...
              'foot_support_qs'},'data/aa_plan.mat');
            end
          end
        end
      end

      % If the action sequence is specified, we need to solve the ZMP
      % planning and IK for the whole sequence.
      if(action_options.ZMP)
          action_sequence_ZMP = action_sequence;
        
        
          dt = 0.02;
          window_size = ceil((action_sequence_ZMP.tspan(end)-action_sequence_ZMP.tspan(1))/dt);
          zmp_planner = ZMPplanner(window_size,r.num_contacts,dt,9.81);
          t_breaks = action_sequence_ZMP.tspan(1)+dt*(0:window_size-1);
          t_breaks(end) = action_sequence_ZMP.tspan(end);
          contact_pos = cell(1,window_size);
          for i = 1:length(t_breaks)
              ikargs = action_sequence_ZMP.getIKArguments(t_breaks(i));
              j = 1;
              while j<length(ikargs)
                  if(ikargs{j} == 0)
                    j = j+2;
                  else
                      contact_pos_ind = (all(ikargs{j+2}.max(1:2,:)==ikargs{j+2}.min(1:2,:),1));
                      contact_pos{i} = [contact_pos{i} ikargs{j+2}.max(1:2,contact_pos_ind)];
                      j = j+3;
                  end
              end
          end
          % TODO: Publish constraint satisfaction message here
          com_height_traj = PPTrajectory(foh(action_sequence.key_time_samples,com_key_time_samples(3,:)));
          com_height = com_height_traj.eval(t_breaks);
          q0 = q;
          qdot0 = zeros(size(q));
          com0 = r.getCOM(q0);
          comdot0 = 0*com0;
          zmp_options = struct();
          zmp_options.supportPolygonConstraints = false;
          zmp_options.shrink_factor = 0.8;
          zmp_options.useQP = true;
          zmp_options.penalizeZMP = true;
          [com_plan,planar_comdot_plan,~,zmp_plan] = zmp_planner.planning(com0(1:2),comdot0(1:2),contact_pos,com_height,t_breaks,zmp_options);
%           q_zmp_plan = zeros(r.getNumDOF,length(t_breaks));
%           q_zmp_plan(:,1) = q0;

          % Add com constraints to action_sequence
          comdot_height_plan = com_height_traj.deriv(t_breaks);
          comdot_plan = [planar_comdot_plan;comdot_height_plan];
          com_traj = PPTrajectory(pchipDeriv(t_breaks,com_plan,comdot_plan));
          
          com_constraint = ActionKinematicConstraint(r,0,zeros(3,1),com_traj, ...
                              action_sequence_ZMP.tspan,'com');
          action_sequence = action_sequence.addKinematicConstraint(com_constraint);
          
          options.qtraj0 = PPTrajectory(spline(action_sequence.key_time_samples,q_key_time_samples));
          options.quasiStaticFlag = false;
          options.nSample = length(t_breaks)-1;
          options.considerStaticContacts = false;
          [t_zmp_breaks, q_zmp_plan, qdot_zmp_plan, qddot_zmp_plan, inverse_kin_sequence_info] = inverseKinSequence(r, q0, qdot0, action_sequence,options);

          % Drake gui playback
          xtraj = PPTrajectory(pchipDeriv(t_zmp_breaks,[q_zmp_plan;qdot_zmp_plan],[qdot_zmp_plan;0*qdot_zmp_plan]));
          xtraj = xtraj.setOutputFrame(r.getStateFrame());
          v.playback(xtraj,struct('slider',true));

          % publish t_breaks, q_zmp_plan with RobotPlanPublisher.java
          constraints_satisfied = ones(max(1,msg.num_contact_goals), ...
            size(q_zmp_plan,2));

          publish(robot_plan_publisher, t_zmp_breaks, ...
            [q_zmp_plan; qdot_zmp_plan], ...
            constraints_satisfied);
        end
        if(action_options.IK)
          % publish robot state message
          ik_time_in_key_samples = (ik_time==action_sequence.key_time_samples);
          if(any(ik_time_in_key_samples))
            q_ik = q_key_time_samples(:,ik_time_in_key_samples);
          else
            ikargs = action_sequence.getIKArguments(ik_time);
            [q_ik,info] = inverseKin(r,q,ikargs{:},options);
          end
          x = [q_ik;0*q_ik];
          v.draw(0,x);
          constraints_satisfied = ones(max(1,msg.num_contact_goals),1);
          publish(robot_state_coder,0,x, ...
            'RESPONSE_IK_SOLUTION_AT_TIME_FOR_ACTION_SEQUENCE', ...
            constraints_satisfied);
        end

      catch ex
        warning on
        warning( [ex.message '\n\nOriginal error message:\n\n\t%s'], ...
          regexprep(ex.getReport,'\n','\n\t'));
        q=q_bk;
        continue;
      end
      if(action_options.run_once)
        break;
      end
      display('Listening ...');
    end
  end

end 

function fprintfVerb(options,varargin)
  if(options.verbose) fprintf(varargin{:}); end
end

function body = findLinkContaining(r,linkname,robot,throw_error)  
  % @param robot can be the robot number or the name of a robot
  % robot=0 means look at all robots
  if nargin<3 || isempty(robot), robot=0; end
  if ischar(robot) robot = strmatch(lower(robot),lower({r.name})); end
  linkname=regexprep(linkname,'\+','\\\+');
  ind = find(cellfun(@(x)~isempty(x),regexp(lower({r.body.linkname}),[lower(linkname) '\+?'])));
  if (robot~=0), ind = ind([r.body(ind).robotnum]==robot); end
  if (length(ind)>1)
    if (nargin<4 || throw_error)
      body = r.body(ind(1));
      warning('Couldn''t find unique link %s. Returning first match: %s', ...
        linkname,body.linkname);
    else 
      body=[];
    end
  elseif (length(ind)<1)
    if (nargin<4 || throw_error)
      error(['couldn''t find link ' ,linkname]);
    else 
      body=[];
    end
  else
    body = r.body(ind);
  end
end

function kc = getConstraintFromGoal(r,goal)
  if(goal.contact_type ~= goal.SUPPORTED_WITHIN_REGION) && (goal.contact_type ~= goal.WITHIN_REGION) && (goal.contact_type ~= goal.COLLISION_AVOIDANCE)
    error('The contact type is not supported yet');
  end
  body_ind = findLink(r,char(goal.object_1_name));
  t_prec = 1e6;
  tspan = round([goal.lower_bound_completion_time goal.upper_bound_completion_time]*t_prec)/t_prec;
  kc_name = [body_ind.linkname,'_from_',num2str(tspan(1)),'_to_',num2str(tspan(2))];
  if(goal.contact_type == goal.COLLISION_AVOIDANCE)
    collision_group_pts = [0;0;0];
    pos = struct();
    pos.max = inf(3,1);
    pos.min = -inf(3,1);
    obstacle = findLink(r,char(goal.object_2_name));
    kc = CollisionAvoidanceConstraint(r,body_ind,tspan,kc_name,obstacle);
  else
    child = findLink(r,char(goal.object_2_name));
    groupname = regexprep(char(goal.object_2_contact_grp),'\/.*','');
    contact_aff = {ContactShapeAffordance(r,child,groupname)};
    collision_group_name = char(goal.object_1_contact_grp);
    collision_group = find(strcmpi(collision_group_name,body_ind.collision_group_name));
    if isempty(collision_group) error('couldn''t find collision group %s on body %s',char(goal.object_1_contact_grp),char(goal.object_1_name)); end
    p=[goal.target_pt.x; goal.target_pt.y; goal.target_pt.z];
    offset = [goal.x_offset; goal.y_offset; goal.z_offset];
    pos = struct();
    pos.max = inf(3,1);
    pos.min = -inf(3,1);
    p = p + offset;
    if(goal.x_relation == 0)
      pos.min(1) = p(1);
      pos.max(1) = p(1);
    elseif(goal.x_relation == 1)
      pos.max(1) = p(1);
      pos.min(1) = -inf;
    elseif(goal.x_relation == 2)
      pos.min(1) = p(1);
      pos.max(1) = inf;
    elseif(goal.x_relation == 3)
      pos.min(1) = -inf;
      pos.max(1) = inf;
    end
    if(goal.y_relation == 0)
      pos.min(2) = p(2);
      pos.max(2) = p(2);
    elseif(goal.y_relation == 1)
      pos.max(2) = p(2);
      pos.min(2) = -inf;
    elseif(goal.y_relation == 2)
      pos.min(2) = p(2);
      pos.max(2) = inf;
    elseif(goal.y_relation == 3)
      pos.min(2) = -inf;
      pos.max(2) = inf;
    end
    if(goal.z_relation == 0)
      pos.min(3) = p(3);
      pos.max(3) = p(3);
    elseif(goal.z_relation == 1)
      pos.max(3) = p(3);
      pos.min(3) = -inf;
    elseif(goal.z_relation == 2)
      pos.min(3) = p(3);
      pos.max(3) = inf;
    elseif(goal.z_relation == 3)
      pos.min(3) = -inf;
      pos.max(3) = inf;
    end
    collision_group_pts = body_ind.getContactPoints(collision_group);
    % If we have multiple contact points in the contact group, we
    % would also constrain that all those contact points are in
    % contact
    num_pts = size(collision_group_pts,2);
    pseudo_Inf = 1e3;
    pos.min(abs(pos.min) > 0.9*pseudo_Inf) = -Inf;
    pos.max(abs(pos.max) > 0.9*pseudo_Inf) = Inf;
    if false && (num_pts>1 && goal.contact_type~=goal.WITHIN_REGION)
      collision_group_pts = [mean(collision_group_pts,2) collision_group_pts];
      pos.max = bsxfun(@times,pos.max,ones(1,num_pts+1));
      pos.max(1:2,2:end) = inf(2,num_pts);
      pos.min = bsxfun(@times,pos.min,ones(1,num_pts+1));
      pos.min(1:2,2:end) = -inf(2,num_pts);
      if(size(pos.max,2) == 6)
        pos.max(4:6,2:end) = inf(3,num_pts);
        pos.min(4:6,2:end) = -inf(3,num_pts);
      end
    else
      pos.min = bsxfun(@times,pos.min,ones(1,num_pts));
      pos.max = bsxfun(@times,pos.max,ones(1,num_pts));
    end
    if(goal.contact_type == goal.SUPPORTED_WITHIN_REGION||goal.contact_type == goal.FORCE_CLOSURE)
      contact_state0 = {ActionKinematicConstraint.MAKE_CONTACT*ones(1,size(collision_group_pts,2))};
      contact_statei = {ActionKinematicConstraint.STATIC_PLANAR_CONTACT*ones(1,size(collision_group_pts,2))};
      contact_statef = {ActionKinematicConstraint.BREAK_CONTACT*ones(1,size(collision_group_pts,2))};
    elseif(goal.contact_type == goal.WITHIN_REGION)
      contact_state0 = {ActionKinematicConstraint.NOT_IN_CONTACT*ones(1,size(collision_group_pts,2))};
      contact_statei = {ActionKinematicConstraint.NOT_IN_CONTACT*ones(1,size(collision_group_pts,2))};
      contact_statef = {ActionKinematicConstraint.NOT_IN_CONTACT*ones(1,size(collision_group_pts,2))};
    end
    contact_distance{1}.min = ConstantTrajectory(zeros(1,size(collision_group_pts,2)));
    contact_distance{1}.max = ConstantTrajectory(zeros(1,size(collision_group_pts,2)));

    %contact_distance{1}.max = ConstantTrajectory(inf(1,size(r.body_pts,2)));
    kc = ActionKinematicConstraint(r,body_ind,collision_group_pts,pos,tspan,kc_name,contact_state0,contact_statei, contact_statef,contact_aff,contact_distance,collision_group_name);
  end

end
