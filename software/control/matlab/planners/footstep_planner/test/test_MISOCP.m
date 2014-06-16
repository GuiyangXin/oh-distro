function test_MIQP()

options.floating = true;
options.dt = 0.001;

warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits')
options.visual = false; % loads faster
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);
r = removeCollisionGroupsExcept(r,{'heel','toe'});
r = compile(r);

request = drc.footstep_plan_request_t();
request.utime = 0;

fp = load(strcat(getenv('DRC_PATH'), '/control/matlab/data/atlas_fp.mat'));
fp.xstar(3) = fp.xstar(3) + 0.50; % make sure we're not assuming z = 0
request.initial_state = r.getStateFrame().lcmcoder.encode(0, fp.xstar);

r = r.setTerrain(KinematicTerrainMap(r, fp.xstar(1:r.getNumDOF), true));

foot_orig = struct('right', [0;-0.15;0;0;0;0], 'left', [0;0.15;0;0;0;0]);

safe_regions = struct('A', {}, 'b', {}, 'point', {}, 'normal', {});
n_regions = 10;
lb = [0;-.2;-0.05];
ub = [2;2.2;0.05];
stone_scale = 0.3;
if 0
  for j = 1:n_regions
    stone = rand(3,1) .* (ub - lb) + lb;
    [Ai, bi] = poly2lincon(stone(1) + stone_scale*[-1, -1, 1, 1],...
                           stone(2) + stone_scale*[-1, 1, 1, -1]);
    Ai = [Ai, zeros(size(Ai, 1), 1)];
    safe_regions(end+1) = struct('A', Ai, 'b', bi, 'point', [0;0;stone(3)], 'normal', [0;0;1]);
  end
%   [Ai, bi] = poly2lincon([2, 7, 7, 2], [.3, .3, -.3, -.3]);
%   Ai = [Ai, zeros(size(Ai, 1), 1)];
%   safe_regions(end+1) = struct('A', Ai, 'b', bi, 'point', [0;0;stones(3)], 'normal', [0;0;1]);
else
  [Ai, bi] = poly2lincon([-.2, -.2, 5,5], [-2, 2, 2, -2]);
  Ai = [Ai, zeros(size(Ai, 1), 1)];
  safe_regions(1) = struct('A', Ai, 'b', bi, 'point', [0;0;0], 'normal', [0;0;1]);
end

goal_pos = struct('right', [1;1-0.15;0.1;0;0;pi],...
                  'left',  [1;1+0.15;0.1;0;0;pi]);


request.params = drc.footstep_plan_params_t();
request.params.max_num_steps = 10;
request.params.min_num_steps = 0;
request.params.min_step_width = 0.25;
request.params.nom_step_width = 0.26;
request.params.max_step_width = 0.27;
request.params.nom_forward_step = 0.25;
request.params.max_forward_step = 0.4;
request.params.nom_upward_step = 0.25;
request.params.nom_downward_step = 0.15;
request.params.ignore_terrain = true;
request.params.planning_mode = drc.footstep_plan_params_t.MODE_AUTO;
request.params.behavior = drc.footstep_plan_params_t.BEHAVIOR_BDI_STEPPING;
request.params.map_command = 0;
request.params.leading_foot = drc.footstep_plan_params_t.LEAD_LEFT;

weights = struct('relative', [10;10;10;0;0;.5],...
                 'relative_final', [100;100;100;0;0;100],...
                 'goal', [100;100;0;0;0;1000]);

tic
nsteps = 10;
seed_plan = FootstepPlan.blank_plan(r, nsteps, [r.foot_bodies_idx.right, r.foot_bodies_idx.left], request.params, safe_regions);
seed_plan.footsteps(1).pos = Point(seed_plan.footsteps(1).frames.center, foot_orig.right);
seed_plan.footsteps(2).pos = Point(seed_plan.footsteps(2).frames.center, foot_orig.left);
plan = footstepMISOCP_conservative(r, seed_plan, weights, goal_pos, 3, 30);
toc

steps = plan.step_matrix();
step_vect = encodeCollocationSteps(steps(:,2:end));
[steps, steps_rel] = decodeCollocationSteps(step_vect);
steps
steps_rel

figure(1);
clf
nsteps = length(plan.footsteps);
r_ndx = 2:2:nsteps;
l_ndx = 1:2:nsteps;
steps = plan.step_matrix();
quiver(steps(1,r_ndx), steps(2, r_ndx), cos(steps(6,r_ndx)), sin(steps(6,r_ndx)), 'b', 'AutoScaleFactor', 0.2)
hold on
quiver(steps(1,l_ndx), steps(2,l_ndx), cos(steps(6,l_ndx)), sin(steps(6,l_ndx)), 'r', 'AutoScaleFactor', 0.2)
plot(steps(1,:), steps(2,:), 'k:')
for j = 1:length(safe_regions)
  V = iris.thirdParty.polytopes.lcon2vert(safe_regions(j).A(:,1:2), safe_regions(j).b);
  k = convhull(V(:,1), V(:,2));
  patch(V(k,1), V(k,2), 'k', 'FaceAlpha', 0.2);
end
axis equal