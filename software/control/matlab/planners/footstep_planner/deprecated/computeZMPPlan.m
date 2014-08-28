function [xtraj,qtraj] = computeZMPPlan(biped, x0, zmptraj, lfoottraj, rfoottraj, ts)

q0 = x0(1:end/2);
%% covert ZMP plan into COM plan using LIMP model
addpath(fullfile(getDrakePath,'examples','ZMP'));
[com,Jcom] = getCOM(biped.manip,q0);
comdot = Jcom*x0(getNumPositions(biped.manip)+(1:getNumPositions(biped.manip)));
limp = LinearInvertedPendulum(com(3,1));

comtraj = [ ZMPplanner(limp,com(1:2),comdot(1:2),setOutputFrame(zmptraj,desiredZMP)); ...
  ConstantTrajectory(com(3,1)) ];

%% compute joint positions with inverse kinematics

ind = getActuatedJoints(biped.manip);
rfoot_body = biped.manip.findLink(biped.r_foot_name);
lfoot_body = biped.manip.findLink(biped.l_foot_name);

cost = Point(biped.manip.getStateFrame,1);
cost.pelvis_x = 0;
cost.pelvis_y = 0;
cost.pelvis_z = 0;
cost.pelvis_roll = 1000;
cost.pelvis_pitch = 1000;
cost.pelvis_yaw = 0;
cost.back_bky = 100;
cost.back_bkx = 100;
cost = double(cost);
options = struct();
options.Q = diag(cost(1:biped.manip.getNumPositions));
options.q_nom = q0;

disp('computing ik...')
for i=1:length(ts)
  t = ts(i);
  if (i>1)
    q(:,i) = inverseKin(biped.manip,q(:,i-1),0,comtraj.eval(t),rfoot_body,[0;0;0],rfoottraj.eval(t),lfoot_body,[0;0;0],lfoottraj.eval(t),options);
  else
    q = q0;
  end
  q_d(:,i) = q(ind,i);
  biped.visualizer.draw(t,q(:,i));
end

xtraj = setOutputFrame(PPTrajectory(spline(ts,[q;0*q])),getOutputFrame(biped.manip));
qtraj = PPTrajectory(spline(ts,q));