classdef VelocityOutputIntegratorBlock < MIMODrakeSystem
  % integrates desired generalized accelerations and outputs a desired velocity
  %
  % input: x, qdd, foot_contact
  % state: [fc_left; fc_right; t_prev; eta; qd_int]
  % output: qd_err (input frame)
  properties
    nq;
    r_ankle_idx;
    l_ankle_idx;
    leg_idx;
    r_leg_idx;
    l_leg_idx;
    act_idx_map;
    zero_ankles_on_contact;
    eta; % gain for leaky integrator
  end
  
  methods
    function obj = VelocityOutputIntegratorBlock(r,options)
      typecheck(r,'Atlas');

      num_q = getNumPositions(r);
      input_frame = MultiCoordinateFrame({getStateFrame(r),AtlasCoordinates(r),FootContactState});
      output_frame = AtlasVelocityRef(r);      
      obj = obj@MIMODrakeSystem(0,4+num_q,input_frame,output_frame,true,true);
      obj = setInputFrame(obj,input_frame);
      obj = setOutputFrame(obj,output_frame);

      obj.nq = num_q;

      if nargin<2
        options = struct();
      end
      
      if isfield(options,'zero_ankles_on_contact')
        typecheck(options.zero_ankles_on_contact,'logical');
        sizecheck(options.zero_ankles_on_contact,[1 1]);
        obj.zero_ankles_on_contact = options.zero_ankles_on_contact;
      else
        obj.zero_ankles_on_contact = true;
      end
      
      if isfield(options,'eta')
        typecheck(options.eta,'double');
        sizecheck(options.eta,[1 1]);
        obj.eta = options.eta;
      else
        obj.eta = 0.0;
      end

      if isfield(options,'dt')
        typecheck(options.dt,'double');
        sizecheck(options.dt,[1 1]);
        dt = options.dt;
      else
        dt = 0.001;
      end
      obj = setSampleTime(obj,[dt;0]); % sets controller update rate
      
			obj.r_ankle_idx = findPositionIndices(r,'r_leg_ak');
			obj.l_ankle_idx = findPositionIndices(r,'l_leg_ak');
			obj.leg_idx = findPositionIndices(r,'leg');
			obj.r_leg_idx = findPositionIndices(r,'r_leg');
			obj.l_leg_idx = findPositionIndices(r,'l_leg');
			obj.act_idx_map = getActuatedJoints(r);
		end
   
    function y=mimoOutput(obj,~,state,varargin)
      x = varargin{1};
      qd = x(obj.nq+1:end);
			fc = varargin{3};
			
			l_foot_contact = fc(1);
			r_foot_contact = fc(2);
			
      % compute desired velocity
			qd_int = state(5:end);

			qd_err = qd_int-qd;

			% do not velocity control ankles when in contact
      if obj.zero_ankles_on_contact && l_foot_contact>0.5
        qd_err(obj.l_ankle_idx)=0;
      end
      if obj.zero_ankles_on_contact && r_foot_contact>0.5
        qd_err(obj.r_ankle_idx)=0;
      end
			
      delta_max = 1.0;
      y = max(-delta_max,min(delta_max,qd_err(obj.act_idx_map)));
		end
		
		function next_state=mimoUpdate(obj,t,state,varargin)
      x = varargin{1};
      qd = x(obj.nq+1:end);
			qdd = varargin{2};
			fc = varargin{3};

			l_foot_contact = fc(1);
			r_foot_contact = fc(2);

 			eta = obj.eta;%state(4);

			qd_int = state(5:end);
			dt = t-state(3);
			
			qd_int = ((1-eta)*qd_int + eta*qd) + qdd*dt; 
			
      if obj.zero_ankles_on_contact && l_foot_contact>0.5
        qd_int(obj.l_ankle_idx)=0;
      end
      if obj.zero_ankles_on_contact && r_foot_contact>0.5
        qd_int(obj.r_ankle_idx)=0;
      end

			next_state = state;
			next_state(1) = fc(1);
			next_state(2) = fc(2);
			next_state(3) = t;

      % eta = max(0.0,eta-dt); % linear ramp
      if state(1)~=l_foot_contact
        % contact state changed, reset integrated velocities
        qd_int(obj.l_leg_idx) = qd(obj.l_leg_idx);
      end
      if state(2)~=r_foot_contact
        % contact state changed, reset integrated velocities
        qd_int(obj.r_leg_idx) = qd(obj.r_leg_idx);
      end
      next_state(4) = eta;
			next_state(5:end) = qd_int;
		end
	end
  
end
