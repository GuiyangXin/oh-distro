classdef CombinedPlanner
  properties
    biped
    footstep_planner
    iris_planner
    walking_planner
    monitors
    request_channels
    handlers
    response_channels
    lc
  end

  methods (Static)
    function r = constructAtlas()
      options.floating = true;
      options.dt = 0.001;
      warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
      warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
      options.visual = false; % loads faster
      r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);
      r = removeCollisionGroupsExcept(r,{'heel','toe'});
      if options.enable_terrainmaps
        r = setTerrain(r,DRCTerrainMap(false,struct('name','Foot Plan','status_code',6,'fill', true,'normal_radius',2,'normal_method','ransac','auto_request',true)));
      end
      r = compile(r);
    end

    function obj = withAtlas()
      obj = CombinedPlanner(CombinedPlanner.constructAtlas());
    end

    function r = constructValkyrie()
      options.floating = true;
      options.dt = 0.001;
      warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
      warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
      options.visual = false; % loads faster
      r = Valkyrie([], options);
      r = removeCollisionGroupsExcept(r,{'heel','toe'});
      if options.enable_terrainmaps
        r = setTerrain(r,DRCTerrainMap(false,struct('name','Foot Plan','status_code',6,'fill', true,'normal_radius',2,'normal_method','ransac','auto_request',true)));
      end
      r = compile(r);
    end

    function obj = withValkyrie()
      obj = CombinedPlanner(CombinedPlanner.constructValkyrie());
    end
  end

  methods
    function obj = CombinedPlanner(biped)
      if nargin < 1
        biped = CombinedPlanner.constructAtlas();
      end

      obj.biped = biped;
      obj.footstep_planner = StatelessFootstepPlanner();
      obj.walking_planner = StatelessWalkingPlanner();
      obj.monitors = {};
      obj.request_channels = {};
      obj.handlers = {};
      obj.response_channels = {};
      obj.lc = lcm.lcm.LCM.getSingleton();

      obj.monitors{end+1} = drake.util.MessageMonitor(drc.footstep_plan_request_t, 'utime');
      obj.request_channels{end+1} = 'FOOTSTEP_PLAN_REQUEST';
      obj.handlers{end+1} = @obj.plan_footsteps;
      obj.response_channels{end+1} = 'FOOTSTEP_PLAN_RESPONSE';

%       obj.monitors{end+1} = drake.util.MessageMonitor(drc.footstep_check_request_t, 'utime');
%       obj.request_channels{end+1} = 'FOOTSTEP_CHECK_REQUEST';
%       obj.handlers{end+1} = @obj.check_footsteps;
%       obj.response_channels{end+1} = 'FOOTSTEP_PLAN_RESPONSE';

      obj.monitors{end+1} = drake.util.MessageMonitor(drc.walking_plan_request_t, 'utime');
      obj.request_channels{end+1} = 'WALKING_TRAJ_REQUEST';
      obj.handlers{end+1} = @obj.plan_walking_traj;
      obj.response_channels{end+1} = 'WALKING_TRAJ_RESPONSE';

      obj.monitors{end+1} = drake.util.MessageMonitor(drc.walking_plan_request_t, 'utime');
      obj.request_channels{end+1} = 'WALKING_CONTROLLER_PLAN_REQUEST';
      obj.handlers{end+1} = @obj.plan_walking_controller;
      obj.response_channels{end+1} = 'WALKING_CONTROLLER_PLAN_RESPONSE';

      obj.monitors{end+1} = drake.util.MessageMonitor(drc.robot_plan_t, 'utime');
      obj.request_channels{end+1} = 'COMMITTED_ROBOT_PLAN';
      obj.handlers{end+1} = @obj.configuration_traj;
      obj.response_channels{end+1} = 'CONFIGURATION_TRAJ';

    end

    function run(obj)
      for j = 1:length(obj.monitors)
        obj.lc.subscribe(obj.request_channels{j}, obj.monitors{j});
      end
      disp('Combined Planner: ready for plan requests');
      while true
        for j = 1:length(obj.monitors)
          msg = obj.monitors{j}.getNextMessage(5);
          if isempty(msg)
            continue
          end
          plan = obj.handlers{j}(msg);
          obj.lc.publish(obj.response_channels{j}, plan.toLCM());
        end
      end
    end

    function plan = plan_footsteps(obj, msg)
%       profile on
      msg = drc.footstep_plan_request_t(msg);
      plan = obj.footstep_planner.plan_footsteps(obj.biped, msg);
%       profile viewer
    end

    function plan = plan_walking_traj(obj, msg)
      msg = drc.walking_plan_request_t(msg);
      plan = obj.walking_planner.plan_walking(obj.biped, msg, true);
    end

    function plan = plan_walking_controller(obj, msg)
      msg = drc.walking_plan_request_t(msg);
      plan = obj.walking_planner.plan_walking(obj.biped, msg, false);
    end
    
    function plan = configuration_traj(obj, msg)
      msg = drc.robot_plan_t(msg);
      nq = getNumDOF(obj.biped);
      joint_names = obj.biped.getStateFrame.coordinates(1:nq);
      [xtraj,ts] = RobotPlanListener.decodeRobotPlan(msg,true,joint_names); 
      qtraj_pp = spline(ts,[zeros(nq,1), xtraj(1:nq,:), zeros(nq,1)]);
      plan = ConfigurationTraj(qtraj_pp);
    end
end
end





