function runWrenchHandMassCompensationTest()

options.floating = true;
options.dt = 0.001;
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf'),options);

% atlas state subscriber
state_frame = r.getStateFrame();
state_frame.subscribe('EST_ROBOT_STATE');

x0 = getInitialState(r);
q0 = x0(1:getNumPositions(r));

hardware_mode = 2; % 1-sim mode
l_issandia = true;
r_issandia = true;
wrench_manager = WrenchMeasurementHandler(r,hardware_mode);
wrench_manager.setHandType(l_issandia,r_issandia);

ee_wrench_listener = EEWrenchListener('EST_ROBOT_STATE');
msg_timeout = 5; % ms
running_avg=zeros(6,1);
while(1)
    
    [x,ts] = getNextMessage(state_frame,msg_timeout);
    if (~isempty(x))
        %disp('Robot state received.');
        x0 = x;
    end
    
    w = ee_wrench_listener.getNextMessage(msg_timeout);
    if (~isempty(w))
        disp('Wrenches Received');
        disp([w.lh(:)'])%norm(w.lh(1:3)) norm(w.lh(4:6)) norm(w.rh(1:3)) norm(w.rh(4:6))       
        wnew = wrench_manager.compensate_hand_wrenches_for_hand_mass_and_sensor_offset(x0,w);
        disp([wnew.lh(:)' norm(wnew.lh(1:3)) norm(wnew.lh(4:6))])% norm(wnew.lh(1:3)) norm(wnew.lh(4:6)) norm(wnew.rh(1:3)) norm(wnew.rh(4:6))
        %w_sensor_frame_offset = wrench_manager.get_dc_wrench_offset_in_sensor_frame(x0,w);
        %running_avg=0.5*w_sensor_frame_offset.lh(:)+0.5*running_avg;
        %disp(running_avg')
    end
end

end
