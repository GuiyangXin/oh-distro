classdef BDIManipCommandBlock < MIMODrakeSystem
  % passes the robot coordinates and converts
  % planned motions in the pelvis to commands to the BDI manip mode
  % controller. these are published directly in the output function.  
  
  properties
    robot;
    params_pub;
    params_listener;
    plan_adjust_listener;
    controller_data;
  end
  
  methods
    function obj = BDIManipCommandBlock(r,controller_data,options)
      typecheck(r,'Atlas');
      typecheck(controller_data,'AtlasManipControllerData');
  
      if nargin<3
        options = struct();
      else
        typecheck(options,'struct');
      end

      coords = AtlasCoordinates(r);
      input_frame = MultiCoordinateFrame({coords,r.getStateFrame});
      
      obj = obj@MIMODrakeSystem(0,0,input_frame,coords,true,true);
      obj = setInputFrame(obj,input_frame);
      obj = setOutputFrame(obj,coords);

      if isfield(options,'dt')
        typecheck(options.dt,'double');
        sizecheck(options.dt,[1 1]);
        dt = options.dt;
      else
        dt = 0.003;
      end
      
      obj.robot = r;
      obj = setSampleTime(obj,[dt;0]); % sets controller update rate
      obj.controller_data = controller_data;
      obj.params_pub = AtlasManipParamsPublisher('ATLAS_MANIPULATE_PARAMS');
      obj.params_listener = AtlasManipParamsListener('ATLAS_STATUS');
      obj.plan_adjust_listener = PlanAdjustModeListener('PLAN_USING_BDI_HEIGHT');
    end
   
    function y=mimoOutput(obj,t,~,varargin)
      q_des=varargin{1};
      x=varargin{2};
      qtraj = obj.controller_data.qtraj;

      plan_adjust = obj.plan_adjust_listener.getNextMessage(0);
      if ~isempty(plan_adjust)
        if plan_adjust.mode == 1
          obj.controller_data.enable_bdi_manip = true;
          send_status(3, 0, 0, 'Atlas controller will stream BDI manip parameters.');
        else
          obj.controller_data.enable_bdi_manip = false;
          send_status(3, 0, 0, 'Atlas controller will NOT stream BDI manip parameters.');
        end
      end
      
      if obj.controller_data.enable_bdi_manip && isa(qtraj,'PPTrajectory') && t<=qtraj.tspan(end)
        cur_params = obj.params_listener.getMessage();
        
        % only support pelvis height for the time being
        foot_z = getFootHeight(obj.robot,x(1:getNumPositions(obj.robot)));
        params.pelvis_height = max(obj.robot.pelvis_min_height, ...
          min(obj.robot.pelvis_max_height,q_des(3)-foot_z));
        params.pelvis_yaw = cur_params.pelvis_yaw;
        params.pelvis_pitch = cur_params.pelvis_pitch;
        params.pelvis_roll = cur_params.pelvis_roll;
        params.com_v0 = cur_params.com_v0;
        params.com_v1 = cur_params.com_v1;
        obj.params_pub.publish(params);
      end
      
      y = q_des;
    end
  end
  
end
