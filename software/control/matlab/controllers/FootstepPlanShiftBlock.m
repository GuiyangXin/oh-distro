classdef FootstepPlanShiftBlock < DrakeSystem
  properties
    dt;
    controller_data; % pointer to shared data handle containing foot trajectories
    robot;
    nq;
    rfoot_idx;
    lfoot_idx;
    lc;
    contact_est_monitor;
    lfoot_contact_state = 0; 
    rfoot_contact_state = 0; 
  end
  
  methods
    function obj = FootstepPlanShiftBlock(r,controller_data,options)
      typecheck(r,'Atlas');
      typecheck(controller_data,'SharedDataHandle');
            
      input_frame = getStateFrame(r);
      output_frame = getStateFrame(r);
      obj = obj@DrakeSystem(0,0,input_frame.dim,output_frame.dim,true,true);
      obj = setInputFrame(obj,input_frame);
      obj = setOutputFrame(obj,output_frame);

      obj.controller_data = controller_data;
      obj.nq = getNumDOF(r);

      if nargin<3
        options = struct();
      end
        
      if isfield(options,'dt')
        typecheck(options.dt,'double');
        sizecheck(options.dt,[1 1]);
        obj.dt = options.dt;
      else
        obj.dt = 0.2;
      end
      
%       obj = setSampleTime(obj,[obj.dt;0]); % sets controller update rate

      obj.lc = lcm.lcm.LCM.getSingleton();
      obj.rfoot_idx = findLinkInd(r,'r_foot');
      obj.lfoot_idx = findLinkInd(r,'l_foot');

      obj.contact_est_monitor = drake.util.MessageMonitor(drc.foot_contact_estimate_t,'utime');
      obj.lc.subscribe('FOOT_CONTACT_ESTIMATE',obj.contact_est_monitor);
      
      obj.robot = r;
    end
    
    function y=output(obj,t,~,x)
      persistent last_t;
      if (isempty(last_t) || last_t > t)
        last_t = 0;
      end
      if (t - last_t >= obj.dt)
        last_t = t;
        contact_data = obj.contact_est_monitor.getNextMessage(0);
        if ~isempty(contact_data)
          msg = drc.foot_contact_estimate_t(contact_data);
          cdata = obj.controller_data.data;
          %t = t + cdata.t_offset;

          if msg.left_contact>0.5 %&& ~lfoot_contact_state
            % left foot coming into contact
            q = x(1:obj.nq); 
            kinsol = doKinematics(obj.robot,q,false,true);

            constraint_ndx = [cdata.link_constraints.link_ndx] == obj.lfoot_idx & all(bsxfun(@eq, [cdata.link_constraints.pt], [0;0;0]));
            lfoot_des = fasteval(cdata.link_constraints(constraint_ndx).traj,t);
            lfoot_act = forwardKin(obj.robot,kinsol,obj.lfoot_idx,[0;0;0],0);
            cdata.trans_drift = lfoot_des(1:3) - lfoot_act(1:3);

%             fprintf('LF:Footstep desired minus actual: x:%2.4f y:%2.4f z:%2.4f m \n',cdata.trans_drift);
            obj.controller_data.setField('trans_drift', cdata.trans_drift);
          elseif msg.right_contact>0.5% && ~rfoot_contact_state
            % right foot coming into contact
            q = x(1:obj.nq); 
            kinsol = doKinematics(obj.robot,q,false,true);

            constraint_ndx = [cdata.link_constraints.link_ndx] == obj.rfoot_idx & all(bsxfun(@eq, [cdata.link_constraints.pt], [0;0;0]));
            rfoot_des = fasteval(cdata.link_constraints(constraint_ndx).traj,t);
            rfoot_act = forwardKin(obj.robot,kinsol,obj.rfoot_idx,[0;0;0],0);
            cdata.trans_drift = rfoot_des(1:3) - rfoot_act(1:3);

%             fprintf('RF:Footstep desired minus actual: x:%2.4f y:%2.4f z:%2.4f m \n',cdata.trans_drift);
            obj.controller_data.setField('trans_drift', cdata.trans_drift);
          end
        end
      end
      y=x;
    end
  end
  
end
