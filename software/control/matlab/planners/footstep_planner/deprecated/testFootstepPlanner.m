options.floating = true;
options.dt = 0.001;
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf'),options);
r = removeCollisionGroupsExcept(r,{'heel','toe'});
r = compile(r);
d = load('../data/atlas_fp.mat');
xstar = d.xstar;
r = r.setInitialState(xstar);
nq = getNumPositions(r);
x0 = xstar;
qstar = xstar(1:nq);
% biped = Biped(r);

% poses = [[0;1;0;0;0;pi/2], [1;1;0;0;0;0], [1;0;0;0;0;0]];
% poses = [[0;1;0;0;0;pi/2], [1;1;0;0;0;0]];
% poses = [3;3;0;0;0;-pi/4];
% poses = [0; -.4; 0;0;0;0];
% poses = [0;1;0;0;0;0];

% Sidestep
% poses = [0; 0.4; 0;0;0;0];


% Walk straight forward
% poses = [1;0;0;0;0;0];

% Diagonal
% poses = [1;1;0;0;0;0];

% Left turn, then straight
poses = [0;1;0;0;0;pi/2];

[xtraj, qtraj] = r.walkingPlan(x0, qstar, [poses(:,end); 10]);

visualizer = r.constructVisualizer();
visualizer.playback(xtraj, struct('slider', true));