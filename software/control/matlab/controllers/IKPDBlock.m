classdef IKPDBlock < MIMODrakeSystem
  % outputs a desired q_ddot (including floating dofs)
  properties
    nq;
    Kp;
    Kd;
    controller_data; % pointer to shared data handle com/foot trajectories
    ikoptions;
    robot;
    max_nrm_err;
		input_foot_contacts;
		r_ankle_idx;
		l_ankle_idx;
    hips
    knees
  end
  
  methods
    function obj = IKPDBlock(r,controller_data,options)
      typecheck(r,'Atlas');
      typecheck(controller_data,'SharedDataHandle');
      
			if nargin<3
				options = struct();
			end

			if ~isfield(options,'input_foot_contacts')
				options.input_foot_contacts = false;
			else
				typecheck(options.input_foot_contacts,'logical');
			end
			
			if options.input_foot_contacts
				input_frame = MultiCoordinateFrame({AtlasCoordinates(r),r.getStateFrame,FootContactState});
			else
				input_frame = MultiCoordinateFrame({AtlasCoordinates(r),r.getStateFrame});
			end
			coords = AtlasCoordinates(r);
      obj = obj@MIMODrakeSystem(0,0,input_frame,coords,true,true);
      obj = setInputFrame(obj,input_frame);
      obj = setOutputFrame(obj,coords);

      obj.controller_data = controller_data;
      obj.nq = getNumDOF(r);
			obj.input_foot_contacts = options.input_foot_contacts;
      
      if isfield(options,'Kp')
        typecheck(options.Kp,'double');
        sizecheck(options.Kp,[obj.nq 1]);
        obj.Kp = options.Kp;
      else
        obj.Kp = 160.0*ones(obj.nq,1);
      end        
      obj.Kp([1,2,6]) = 0; % ignore x,y,yaw
        
      if isfield(options,'Kd')
        typecheck(options.Kd,'double');
        sizecheck(options.Kd,[obj.nq 1]);
        obj.Kd = options.Kd;
      else
        obj.Kd = 19.0*ones(obj.nq,1);
      end
      obj.Kd([1,2,6]) = 0; % ignore x,y,yaw
            
      if isfield(options,'dt')
        typecheck(options.dt,'double');
        sizecheck(options.dt,[1 1]);
        dt = options.dt;
      else
        dt = 0.001;
      end
      obj = setSampleTime(obj,[dt;0]); % sets controller update rate

			obj.r_ankle_idx = findJointIndices(r,'r_leg_ak');
			obj.l_ankle_idx = findJointIndices(r,'l_leg_ak');
            
      if ~isfield(obj.controller_data,'trans_drift')
        obj.controller_data.setField('trans_drift',[0;0;0]);
      end
      
      % setup IK parameters
      cost = Point(r.getStateFrame,1);
      cost.base_x = 0;
      cost.base_y = 0;
      cost.base_z = 0;
      cost.base_roll = 1000;
      cost.base_pitch = 1000;
      cost.base_yaw = 0;
      cost.back_bkz = 10;
      cost.back_bky = 10;
      cost.back_bkx = 10;
      cost.l_leg_hpz = 10;
      cost.r_leg_hpz = 10;
      cost.l_leg_kny = 5;
      cost.r_leg_kny = 5;

      cost = double(cost);
      
      obj.ikoptions = struct();
      obj.ikoptions.Q = diag(cost(1:obj.nq));

      obj.robot = r;
      obj.max_nrm_err = 1.5;      
      obj.hips = findJointIndices(r,'leg_hp');
      obj.knees = findJointIndices(r,'kny');
    end
   
    function y=mimoOutput(obj,t,~,varargin)      
%       persistent qd_filt
      
%       if isempty(qd_filt)
%         qd_filt = 0;
%       end
      
      obj.ikoptions.q_nom = varargin{1};

			x = varargin{2};
      q = x(1:obj.nq);
      qd = x(obj.nq+1:end);

      if obj.input_foot_contacts
				fc = varargin{3};
				
        if fc(1) > 0.5 % left foot in contact
          obj.Kp(obj.l_ankle_idx) = 20;
          obj.Kd(obj.l_ankle_idx) = 3;
        end
        if fc(2) > 0.5 % right foot in contact
          obj.Kp(obj.r_ankle_idx) = 20;
          obj.Kd(obj.r_ankle_idx) = 3;
        end
      end
			
			cdata = obj.controller_data.data;

      approx_args = {};
      for j = 1:length(cdata.link_constraints)
        if ~isempty(cdata.link_constraints(j).traj)
          pos = fasteval(cdata.link_constraints(j).traj,t);
%           pos(3) = pos(3) - cdata.trans_drift(3);
          pos(1:3) = pos(1:3) - cdata.trans_drift;
          approx_args(end+1:end+3) = {cdata.link_constraints(j).link_ndx, cdata.link_constraints(j).pt, pos};
        end
      end
      
      % note: we should really only try to control COM position when in
      % contact with the environment
      com = fasteval(cdata.comtraj,t);
      compos = [com(1:2) - cdata.trans_drift(1:2);nan];

      q_des = linearIK(obj.robot,q,0,compos,approx_args{:},obj.ikoptions);
      
      err_q = q_des - q;
      nrmerr = norm(err_q,1);
      if nrmerr > obj.max_nrm_err
        err_q = obj.max_nrm_err * err_q / nrmerr;
      end
     
%       qd_filt = 0.85*qd_filt + 0.15*qd;
      
%       qd(obj.hips) = qd_filt(obj.hips);
%       qd(obj.knees) = qd_filt(obj.knees);
      
			y = max(-100*ones(obj.nq,1),min(100*ones(obj.nq,1),obj.Kp.*err_q - obj.Kd.*qd));
		end
  end
  
end
