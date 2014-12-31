classdef AtlasPosVelTorqueRef < LCMCoordinateFrame & Singleton
  % atlas input coordinate frame
  methods
    function obj=AtlasPosVelTorqueRef(r)
      typecheck(r,'TimeSteppingRigidBodyManipulator');
      
      nu = getNumInputs(r);
      dim = 3*nu;
      
      obj = obj@LCMCoordinateFrame('AtlasPosVelTorqueRef',dim,'x');
      obj = obj@Singleton();
      
      if isempty(obj.lcmcoder)  % otherwise I had a singleton
        input_names = r.getInputFrame().coordinates;
        input_names = regexprep(input_names,'_motor',''); % remove motor suffix
        input_frame = getInputFrame(r);
        input_frame.setCoordinateNames(input_names); % note: renaming input coordinates
      
        gains = getAtlasGains();
      
        coder = drc.control.AtlasCommandCoder(input_names,r.atlas_version,gains.k_q_p,gains.k_q_i,...
          gains.k_qd_p,gains.k_f_p,gains.ff_qd,gains.ff_qd_d,gains.ff_f_d,gains.ff_const);
        setLCMCoder(obj,JLCMCoder(coder));
      
        coords = input_names;
        coords = vertcat(coords,cellfun(@(a) [a,'_dot'],input_names,'UniformOutput',false));
        coords = vertcat(coords,cellfun(@(a) [a,'_effort'],input_names,'UniformOutput',false));
      
        obj.setCoordinateNames(coords);
        obj.setDefaultChannel('ATLAS_COMMAND');
      end
      
      if (obj.mex_ptr==0)
        obj.mex_ptr = AtlasCommandPublisher(input_names,r.atlas_version,gains.k_q_p,gains.k_q_i,...
          gains.k_qd_p,gains.k_f_p,gains.ff_qd,gains.ff_qd_d,gains.ff_f_d,gains.ff_const);
      end
    end
    
    function publish(obj,t,x,channel)
      % short-cut java publish with a faster mex version
      AtlasCommandPublisher(obj.mex_ptr,channel,t,x);
    end
    
    function delete(obj)
      AtlasCommandPublisher(obj.mex_ptr);
    end
    
    function updateGains(obj,gains)
      assert(isfield(gains,'k_q_p'));
      assert(isfield(gains,'k_q_i'));
      assert(isfield(gains,'k_qd_p'));
      assert(isfield(gains,'k_f_p'));
      assert(isfield(gains,'ff_qd'));
      assert(isfield(gains,'ff_qd_d'));
      assert(isfield(gains,'ff_f_d'));
      assert(isfield(gains,'ff_const'));
      
      obj.mex_ptr = AtlasCommandPublisher(obj.mex_ptr,gains.k_q_p,gains.k_q_i,...
          gains.k_qd_p,gains.k_f_p,gains.ff_qd,gains.ff_qd_d,gains.ff_f_d,gains.ff_const);
    end
    
  end
  
  methods (Static)
    function obj = loadobj(a)
      a.mex_ptr = 0;
      obj=a;
    end
  end
  
  properties
    mex_ptr=0;
  end
end
