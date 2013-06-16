classdef CrawlingController < DRCController
    
  properties (SetAccess=protected,GetAccess=protected)
    robot;
    r_foot_idx;
    l_foot_idx;
    r_hand_idx;
    l_hand_idx;
  end
    
  methods
    function obj = CrawlingController(name,r,options)
      typecheck(r,'Atlas');

      %  NOTE: this should only be required for setting the normals to
      %  [0;0;1], and should be removed if we start trusting/using the
      %  terrain
      r = setTerrain(r,RigidBodyTerrain);
      
      ctrl_data = SharedDataHandle(struct('qtraj',zeros(getNumDOF(r),1),...
          'qdtraj',zeros(getNumDOF(r),1),...
          'qddtraj',zeros(getNumDOF(r),1),...
          'support_times',[],...
          'supports',[],...
          'ignore_terrain',true,...
          't_offset',0));
      
      sys = PosVelFeedForwardBlock(r,ctrl_data,options);
      obj = obj@DRCController(name,sys,AtlasState(r));
      obj.robot = r;
      obj.controller_data = ctrl_data;
    
      obj.r_foot_idx = findLinkInd(r,'r_foot');
      obj.l_foot_idx = findLinkInd(r,'l_foot');
      obj.r_hand_idx = findLinkInd(r,'r_hand');
      obj.l_hand_idx = findLinkInd(r,'l_hand');
      
      obj = setTimedTransition(obj,inf,name,false);
      obj = addLCMTransition(obj,'WALKING_PLAN',drc.walking_plan_t(),'crawling');  % for crawling
      obj = addLCMTransition(obj,'STOP_CRAWLING',drc.plan_control_t(),'crawling');  
      obj = addLCMTransition(obj,'RECOVERY_CMD',drc.recovery_t(),'dummy'); % for recovery

    end
        
    function msg = status_message(obj,t_sim,t_ctrl)
      msg = drc.controller_status_t();
      msg.utime = t_sim * 1000000;
      msg.state = msg.CRAWLING;
      msg.controller_utime = t_ctrl * 1000000;
      msg.V = 0;
      msg.Vdot = 0;
    end        

    function obj = initialize(obj,data)

%       load('data/crawling_traj.mat');
%       qdtraj = fnder(q_traj,1);
%       qddtraj = fnder(qdtraj,1);
%       obj.controller_data.setField('qtraj',q_traj);
%       obj.controller_data.setField('qdtraj',qdtraj);
%       obj.controller_data.setField('qddtraj',qddtraj);
%       obj.controller_data.setField('support_times',support_times);
%       obj.controller_data.setField('supports',supports);
% 
%       obj = setDuration(obj,qdtraj.tspan(end),false); % set the controller timeout
% 
%       return;

      if isfield(data,'WALKING_PLAN')
      
        tmp_fname = ['tmp_w_', num2str(feature('getpid')), '.mat'];

        msg_data = data.WALKING_PLAN;
        % do we have to save to file to convert a byte stream to a
        % matlab binary?

        support_times = msg_data.support_times;
        obj.controller_data.setField('support_times',support_times);

        fid = fopen(tmp_fname,'w');
        fwrite(fid,typecast(msg_data.supports,'uint8'),'uint8');
        fclose(fid);
        matdata = load(tmp_fname);
        if iscell(matdata.supports) 
          warning('somebody sent me a cell array of supports.  don''t do that anymore!');
          matdata.supports = [matdata.supports{:}];
        end
        obj.controller_data.setField('supports',matdata.supports);

        fid = fopen(tmp_fname,'w');
        fwrite(fid,typecast(msg_data.qtraj,'uint8'),'uint8');
        fclose(fid);
        matdata = load(tmp_fname);
        qdtraj = fnder(matdata.qtraj,1);
        qddtraj = fnder(qdtraj,1);
        obj.controller_data.setField('qtraj',matdata.qtraj);
        obj.controller_data.setField('qdtraj',qdtraj);
        obj.controller_data.setField('qddtraj',qddtraj);

        t_offset = msg_data.t_offset;
        obj.controller_data.setField('t_offset',t_offset);

        delete(tmp_fname);
        if t_offset < 0
          obj = setDuration(obj,inf,false); % set the controller timeout
        else
          obj = setDuration(obj,qdtraj.tspan(end),false); % set the controller timeout
        end
        

      elseif isfield(data,'STOP_CRAWLING')
        
        r = obj.robot;

        x0 = data.AtlasState;
        q0 = x0(1:getNumDOF(r));

        obj.controller_data.setField('qtraj',ConstantTrajectory(q0));
        obj.controller_data.setField('qdtraj',ConstantTrajectory(0*q0));
        obj.controller_data.setField('qddtraj',ConstantTrajectory(0*q0));
        obj.controller_data.setField('t_offset',0);
        obj.controller_data.setField('support_times',0);
        bodies = [obj.r_foot_idx obj.l_foot_idx obj.r_hand_idx obj.l_hand_idx];
        obj.controller_data.setField('supports',SupportState(r,bodies));
        
        obj = setDuration(obj,inf,false); % set the controller timeout

      else
        % assume we've looped to ourself
        t_offset = obj.controller_data.data.t_offset;
        qtraj = shiftTime(obj.controller_data.data.qtraj,-t_offset);
        qdtraj = shiftTime(obj.controller_data.data.qdtraj,-t_offset);
        qddtraj = shiftTime(obj.controller_data.data.qddtraj,-t_offset);

        obj.controller_data.setField('qtraj',qtraj);
        obj.controller_data.setField('qdtraj',qdtraj);
        obj.controller_data.setField('qddtraj',qddtraj);
        obj.controller_data.setField('t_offset',0);
        obj = setDuration(obj,qdtraj.tspan(end)-t_offset,false); % set the controller timeout
      end

    end
  end
    
end
