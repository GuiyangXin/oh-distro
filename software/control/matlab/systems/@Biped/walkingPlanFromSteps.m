function [support_times, supports, comtraj, foottraj, V, zmptraj] = walkingPlanFromSteps(biped, x0, footsteps,options)

nq = getNumDOF(biped);
q0 = x0(1:nq);
kinsol = doKinematics(biped,q0);

[zmptraj,foottraj, support_times, supports] = planZMPTraj(biped, q0, footsteps,options);
zmptraj = setOutputFrame(zmptraj,desiredZMP);
%% construct ZMP feedback controller
com = getCOM(biped,kinsol);
foot_pos = contactPositions(biped,kinsol,[biped.foot_bodies_idx.right, biped.foot_bodies_idx.left]);
zfeet = mean(foot_pos(3,:));

% get COM traj from desired ZMP traj
options.com0 = com(1:2);
[c,V,comtraj] = LinearInvertedPendulum.ZMPtrackerClosedForm(com(3)-zfeet,zmptraj,options);
