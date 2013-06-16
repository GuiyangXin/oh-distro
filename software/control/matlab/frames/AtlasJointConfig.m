classdef AtlasJointConfig < LCMCoordinateFrameWCoder & Singleton
  % atlas joint position frame (coordinate ordering from atlas state frame, 
  % not input frame)
  methods
    function obj=AtlasJointConfig(r,floating)
      typecheck(r,'TimeSteppingRigidBodyManipulator');
      if nargin>1
        typecheck(floating,'logical');
      else
        floating = true;
      end
      
      nq = r.getNumDOF();
      if floating
        jrange = 7:nq; % ignore floating base dofs
      else
        jrange = 1:nq;
      end
      
      joint_names = r.getStateFrame.coordinates(jrange); 
      obj = obj@LCMCoordinateFrameWCoder('NominalPositionGoal',length(jrange),'x');
      obj = obj@Singleton();
      if isempty(obj.lcmcoder)
        coder = JointAnglesCoder('atlas',joint_names);
        obj = setLCMCoder(obj,JLCMCoder(coder));
        obj.setCoordinateNames(joint_names);
        obj.setDefaultChannel('NOMINAL_POS_GOAL');
      end
    end
  end
end
