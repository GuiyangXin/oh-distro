function calibrateAtlasArmEncoders(runLCM)
%NOTEST

if nargin < 1
  runLCM = false;
end

% silence some warnings
warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits')

% must run this with encoders disabled in the state sync process
r = DRCAtlas();
nq = getNumPositions(r);

state_frame = getStateFrame(r);
state_frame.subscribe('EST_ROBOT_STATE');
ref_frame = drcFrames.AtlasPositionRef(r);

% extra frame contains raw encoder values
extra_frame = LCMCoordinateFrame('drcFrames.AtlasStateExtra',4*28,'x');
coder = drc.control.AtlasStateExtraCoder(28);
extra_frame.setLCMCoder(JLCMCoder(coder));
extra_frame.subscribe('ATLAS_STATE_EXTRA');

joint_index_map = struct(); % maps joint names to indices
for i=1:nq
  joint_index_map.(state_frame.coordinates{i}) = i;
end

act_idx = getActuatedJoints(r);

[jlmin,jlmax] = getJointLimits(r);
delta = 0.2; % [rad] amount to command beyond joint limits

% initial/final configuration
x0 = Point(state_frame);
x0.r_arm_shx = 1.45;
x0.l_arm_shx = -1.45;
x0 = double(x0);
q0 = x0(1:nq);


% calibration configuration: drive arm joints to limits
x_calib = Point(state_frame);
x_calib.r_arm_usy = jlmax(joint_index_map.r_arm_usy) + delta;
x_calib.r_arm_shx = jlmax(joint_index_map.r_arm_shx) + delta;
x_calib.r_arm_ely = jlmin(joint_index_map.r_arm_ely) - delta;
x_calib.r_arm_elx = jlmax(joint_index_map.r_arm_elx) + delta;
x_calib.r_arm_uwy = jlmin(joint_index_map.r_arm_uwy) - delta;
x_calib.r_arm_mwx = jlmin(joint_index_map.r_arm_mwx) - delta;
x_calib.l_arm_usy = jlmax(joint_index_map.l_arm_usy) + delta;
x_calib.l_arm_shx = jlmin(joint_index_map.l_arm_shx) - delta;
x_calib.l_arm_ely = jlmin(joint_index_map.l_arm_ely) - delta;
x_calib.l_arm_elx = jlmin(joint_index_map.l_arm_elx) - delta;
x_calib.l_arm_uwy = jlmin(joint_index_map.l_arm_uwy) - delta;
x_calib.l_arm_mwx = jlmax(joint_index_map.l_arm_mwx) + delta;
x_calib = double(x_calib);
q_calib = x_calib(1:nq);

% "correct" encoder readings at joint limits
calib_val = Point(state_frame);
calib_val.r_arm_usy = 0.79804;
calib_val.r_arm_shx = 1.57022;
calib_val.r_arm_ely = -0.01648;
calib_val.r_arm_elx = -0.00297;
calib_val.r_arm_uwy = 0.02374;
calib_val.r_arm_mwx = -1.22977;
calib_val.l_arm_usy = 0.79858;
calib_val.l_arm_shx = -1.58616;
calib_val.l_arm_ely = -0.04079;
calib_val.l_arm_elx = 0.00481;
calib_val.l_arm_uwy = 0.02515;
calib_val.l_arm_mwx = 1.16325;
calib_val = double(calib_val);

behavior_pub = AtlasBehaviorModePublisher('ATLAS_BEHAVIOR_COMMAND');
behavior_lis = AtlasBehaviorModeListener('ATLAS_STATUS');

lc = lcm.lcm.LCM.getSingleton();
monitor = drake.util.MessageMonitor(drc.utime_t,'utime');
lc.subscribe('CALIBRATE_ARM_ENCODERS',monitor);  
if runLCM % wait for LCM trigger, then run
  while true
    pause(0.25);
    x = monitor.getNextMessage(10);
    if ~isempty(x)
      moveAndWriteCalibration();
    end
  end
else
  moveAndWriteCalibration();
end

  function moveAndWriteCalibration

    switch_back_to_freeze = false;
    % get current atlas behavior
    pause(0.25);
    behavior = behavior_lis.getMessage();
    if behavior == AtlasBehaviorModeListener.BEHAVIOR_FREEZE
      switch_back_to_freeze = true;
      % switch to user mode
      d.utime = 0;
      d.command = 'user';
      behavior_pub.publish(d);
      pause(5.0);
  
      % gross, have to turn the controller off again after user transition
      msg = drc.utime_t();
      msg.utime = 0; % enable with positive utime
      lc.publish('CALIBRATE_ARM_ENCODERS',msg);
      pause(5.0);
      monitor.getNextMessage(10);
    end
    
    msg = drc.utime_t();
    msg.utime = -1; % disable with negative utime
    lc.publish('ENABLE_ENCODERS',msg);
    pause(0.1);

    % move to initial pos
    atlasLinearMoveToPos(q0,state_frame,ref_frame,act_idx,5);

    % move to calib pos
    atlasLinearMoveToPos(q_calib,state_frame,ref_frame,act_idx,7);

    pause(0.1);
    % record encoder values
    [ex,~] = extra_frame.getMessage();

    % move to initial pos again
    atlasLinearMoveToPos(q0,state_frame,ref_frame,act_idx,5);

    % display encoder offsets 
    ex = ex(1:28); % just grab state off the robot
    enc_diff = calib_val(r.stateToBDIInd) - ex;

    % note: using BDI's order, same as in common_components.cfg
    JOINT_L_ARM_USY   = 17;
    JOINT_L_ARM_SHX   = 18;
    JOINT_L_ARM_ELY   = 19;
    JOINT_L_ARM_ELX   = 20;
    JOINT_L_ARM_UWY   = 21;
    JOINT_L_ARM_MWX   = 22;
    JOINT_R_ARM_USY   = 23;
    JOINT_R_ARM_SHX   = 24;
    JOINT_R_ARM_ELY   = 25;
    JOINT_R_ARM_ELX   = 26;
    JOINT_R_ARM_UWY   = 27;
    JOINT_R_ARM_MWX   = 28;

    msg = bot_param.set_t();
    msg.utime = bot_timestamp_now();
    msg.sequence_number = bot_param_get_seqno();
    msg.server_id = bot_param_get_server_id();
    
    joint_ind = bot_param.entry_t();
    joint_ind.is_array = true;
    joint_ind.key = 'control.encoder_offsets.index';
    joint_ind.value = ['3,' ...
                       num2str(JOINT_R_ARM_USY-1) ',' ...
                       num2str(JOINT_R_ARM_SHX-1) ',' ...
                       num2str(JOINT_R_ARM_ELY-1) ',' ...
                       num2str(JOINT_R_ARM_ELX-1) ',' ...
                       num2str(JOINT_R_ARM_UWY-1) ',' ...
                       num2str(JOINT_R_ARM_MWX-1) ',' ...
                       num2str(JOINT_L_ARM_USY-1) ',' ...
                       num2str(JOINT_L_ARM_SHX-1) ',' ...
                       num2str(JOINT_L_ARM_ELY-1) ',' ...
                       num2str(JOINT_L_ARM_ELX-1) ',' ...
                       num2str(JOINT_L_ARM_UWY-1) ',' ...
                       num2str(JOINT_L_ARM_MWX-1)];
    
    offsets = bot_param.entry_t();
    offsets.is_array = true;
    offsets.key = 'control.encoder_offsets.value';
    offsets.value = ['4.24,' ...
                       num2str(enc_diff(JOINT_R_ARM_USY)) ',' ...
                       num2str(enc_diff(JOINT_R_ARM_SHX)) ',' ...
                       num2str(enc_diff(JOINT_R_ARM_ELY)) ',' ...
                       num2str(enc_diff(JOINT_R_ARM_ELX)) ',' ...
                       num2str(enc_diff(JOINT_R_ARM_UWY)) ',' ...
                       num2str(enc_diff(JOINT_R_ARM_MWX)) ',' ...
                       num2str(enc_diff(JOINT_L_ARM_USY)) ',' ...
                       num2str(enc_diff(JOINT_L_ARM_SHX)) ',' ...
                       num2str(enc_diff(JOINT_L_ARM_ELY)) ',' ...
                       num2str(enc_diff(JOINT_L_ARM_ELX)) ',' ...
                       num2str(enc_diff(JOINT_L_ARM_UWY)) ',' ...
                       num2str(enc_diff(JOINT_L_ARM_MWX))];
    
    msg.numEntries = 2;
    msg.entries = javaArray('bot_param.entry_t', msg.numEntries);
    msg.entries(1) = joint_ind;
    msg.entries(2) = offsets;
    lc.publish('PARAM_SET',msg);
    
    disp('Arm encoder calibration completed.');
    send_status(1, 0, 0, 'Arm encoder calibration completed.');

    msg = drc.utime_t();
    msg.utime = 0;
    lc.publish('REFRESH_ENCODER_OFFSETS',msg);

    msg = drc.utime_t();
    msg.utime = 1; % enable with positive utime
    lc.publish('ENABLE_ENCODERS',msg);
    
    if switch_back_to_freeze      
      % switch to user mode
      d.utime = 0;
      d.command = 'freeze';
      behavior_pub.publish(d);
      pause(0.5);
    end

  end


end
