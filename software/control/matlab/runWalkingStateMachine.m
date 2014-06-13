function runWalkingStateMachine(options,state_channel)
% SIMULATION ONLY
addpath(fullfile(getDrakePath,'examples','ZMP'));

options.namesuffix = '';
options.floating = true;
options.ignore_friction = true;

if ~isfield(options,'backup_mode'); options.backup_mode = false; end
if(~isfield(options,'use_hand_ft')); options.use_hand_ft = false; end
if(~isfield(options,'use_mex')); options.use_mex = true; end
if(~isfield(options,'debug')); options.debug = false; end

if (options.use_hand_ft)
  urdf = strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf');
else
  urdf = strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf');
end

r = Atlas(urdf,options);
r = setTerrain(r,DRCTerrainMap(true,struct('name','WalkingStateMachine','fill',true)));
r = removeCollisionGroupsExcept(r,{'toe','heel'});
r = compile(r);

if nargin > 1
  typecheck(state_channel,'char');
  r.getStateFrame.setDefaultChannel(state_channel);
end

standing_controller = StandingManipController('standing',r,options);
walking_controller = WalkingController('walking',r,options);
bracing_controller = BracingController('bracing',r,options);
dummy_controller = DummyController('dummy',r,options);

controllers = struct(standing_controller.name,standing_controller,...
                     walking_controller.name,walking_controller,...     
                     bracing_controller.name,bracing_controller,...
                     dummy_controller.name,dummy_controller);

state_machine = DRCStateMachine(controllers,standing_controller.name);

state_machine.run(options.backup_mode);

end
