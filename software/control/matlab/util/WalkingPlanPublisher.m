classdef WalkingPlanPublisher

	properties
		lc;
		channel;
	end

	methods
		function obj = WalkingPlanPublisher(channel)
			obj.channel = channel;
			obj.lc = lcm.lcm.LCM.getSingleton();
		end

		function publish(obj, utime, data)
      msg = drc.walking_plan_t();
      
 			msg.robot_name = 'atlas';
      msg.utime = utime;
      % assume: data is a struct with fields: htraj, hddtraj, S, s1, s2, 
      %    s1dot, s2dot, support_times, supports, comtraj, zmptraj, link_constraints
      tmp_fname = ['tmp_r_', num2str(feature('getpid')), '.mat'];

      % do we have to save to file to convert to byte stream?
      qtraj = data.qtraj;
      save(tmp_fname,'qtraj');
      fid = fopen(tmp_fname,'r');
      msg.qtraj = fread(fid,inf,'*uint8'); % note: this will be stored as an int8 in the lcmtype
      fclose(fid);
      msg.n_qtraj_bytes = length(msg.qtraj); 

      S = data.S;
      save(tmp_fname,'S');
      fid = fopen(tmp_fname,'r');
      msg.S = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_S_bytes = length(msg.S); 

      s1 = data.s1;
      save(tmp_fname,'s1');
      fid = fopen(tmp_fname,'r');
      msg.s1 = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_s1_bytes = length(msg.s1); 

      s1dot = data.s1dot;
      save(tmp_fname,'s1dot');
      fid = fopen(tmp_fname,'r');
      msg.s1dot = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_s1dot_bytes = length(msg.s1dot); 

      s2 = data.s2;
      save(tmp_fname,'s2');
      fid = fopen(tmp_fname,'r');
      msg.s2 = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_s2_bytes = length(msg.s2); 

      s2dot = data.s2dot;
      save(tmp_fname,'s2dot');
      fid = fopen(tmp_fname,'r');
      msg.s2dot = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_s2dot_bytes = length(msg.s2dot); 

      msg.n_support_times = length(data.support_times);
      msg.support_times = data.support_times;
      
      supports = data.supports;
      save(tmp_fname,'supports');
      fid = fopen(tmp_fname,'r');
      msg.supports = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_supports_bytes = length(msg.supports); 

      comtraj = data.comtraj;
      save(tmp_fname,'comtraj');
      fid = fopen(tmp_fname,'r');
      msg.comtraj = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_comtraj_bytes = length(msg.comtraj); 

      zmptraj = data.zmptraj;
      save(tmp_fname,'zmptraj');
      fid = fopen(tmp_fname,'r');
      msg.zmptraj = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_zmptraj_bytes = length(msg.zmptraj); 

      link_constraints = data.link_constraints;
      save(tmp_fname, 'link_constraints');
      fid = fopen(tmp_fname, 'r');
      msg.link_constraints = fread(fid,inf,'*uint8');
      fclose(fid);
      msg.n_link_constraints_bytes = length(msg.link_constraints);
      
      msg.mu = data.mu;
      msg.ignore_terrain = data.ignore_terrain;
      if isfield(data,'t_offset')
        msg.t_offset = data.t_offset;
      else
        msg.t_offset = 0;
      end

      obj.lc.publish(obj.channel, msg);
      delete(tmp_fname);
	end

end

end
