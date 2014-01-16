package drc.control;

import java.io.*;
import java.lang.*;
import lcm.lcm.*;

public class AtlasCommandCoder implements drake.util.LCMCoder 
{
    // Atlas plugin has a fixed order for joints, 
    //        so number of joints here is fixed
    final int m_num_joints = 28;
		int[] drake_to_atlas_joint_map;

    final int mode; 
    // mode==1: torque-only, 
    // mode==2: position-only, fixed gains
    // mode==3: position and velocity, fixed gains
    // mode==4: position, velocity, torque, fixed pd gains
    // TODO: add additional modes (e.g., position w/variable gains, mixed torque-position control
    
    drc.atlas_command_t msg;

    public AtlasCommandCoder(String[] joint_name, double[] Kp, double[] Kd) throws Exception
    {
      this(joint_name,Kp,Kd,2);
    }
    
    public AtlasCommandCoder(String[] joint_name, double[] Kp, double[] Kd, int send_mode) throws Exception
    {
      this(joint_name, send_mode);
      
      int j;
      for (int i=0; i<msg.num_joints; i++) {
        j = drake_to_atlas_joint_map[i];
        msg.kp_position[j] = Kp[i];
        msg.kd_position[j] = Kd[i];
        if (send_mode>2) msg.kp_velocity[j] = Kd[i];
      }
    }

    public AtlasCommandCoder(String[] joint_name) throws Exception
    {
      this(joint_name,1); //Default mode=1
    }
    
    public AtlasCommandCoder(String[] joint_name, int send_mode) throws Exception
    {
      if (joint_name.length != m_num_joints)
        throw new Exception("Length of joint_name must be " + m_num_joints);
      mode = send_mode;
			// fixed ordering assumed by drcsim interface AND atlas api 
			// see: AtlasControlTypes.h 
      String[] atlas_joint_name = new String[m_num_joints];
      atlas_joint_name[0] = "back_bkz";
      atlas_joint_name[1] = "back_bky";
      atlas_joint_name[2] = "back_bkx";
      atlas_joint_name[3] = "neck_ay";
      atlas_joint_name[4] = "l_leg_hpz";
      atlas_joint_name[5] = "l_leg_hpx";
      atlas_joint_name[6] = "l_leg_hpy";
      atlas_joint_name[7] = "l_leg_kny";
      atlas_joint_name[8] = "l_leg_aky";
      atlas_joint_name[9] = "l_leg_akx";
      atlas_joint_name[10] = "r_leg_hpz";
      atlas_joint_name[11] = "r_leg_hpx";
      atlas_joint_name[12] = "r_leg_hpy";
      atlas_joint_name[13] = "r_leg_kny";
      atlas_joint_name[14] = "r_leg_aky";
      atlas_joint_name[15] = "r_leg_akx";
      atlas_joint_name[16] = "l_arm_usy";
      atlas_joint_name[17] = "l_arm_shx";
      atlas_joint_name[18] = "l_arm_ely";
      atlas_joint_name[19] = "l_arm_elx";
      atlas_joint_name[20] = "l_arm_uwy";
      atlas_joint_name[21] = "l_arm_mwx";
      atlas_joint_name[22] = "r_arm_usy";
      atlas_joint_name[23] = "r_arm_shx";
      atlas_joint_name[24] = "r_arm_ely";
      atlas_joint_name[25] = "r_arm_elx";
      atlas_joint_name[26] = "r_arm_uwy";
      atlas_joint_name[27] = "r_arm_mwx";

			drake_to_atlas_joint_map = new int[m_num_joints];
      
      for (int i=0; i<m_num_joints; i++) {
	      for (int j=0; j<m_num_joints; j++) {
					if (joint_name[i].equals(atlas_joint_name[j]))
						drake_to_atlas_joint_map[i]=j;
				}
			}

      msg = new drc.atlas_command_t();
      msg.num_joints = m_num_joints;
      
      msg.position = new double[msg.num_joints];
    	msg.velocity = new double[msg.num_joints];
      msg.effort = new double[msg.num_joints];

      msg.kp_position = new double[msg.num_joints];
    	msg.ki_position = new double[msg.num_joints];
      msg.kd_position = new double[msg.num_joints];
      msg.kp_velocity = new double[msg.num_joints];

    	msg.i_effort_min = new double[msg.num_joints];
      msg.i_effort_max = new double[msg.num_joints];
      
			msg.k_effort = new byte[msg.num_joints];
			msg.desired_controller_period_ms = 5; // set desired controller rate (ms)

      for (int i=0; i<msg.num_joints; i++) {
        msg.position[i] = 0.0;
        msg.velocity[i] = 0.0;
        msg.effort[i] = 0.0;
        msg.kp_position[i] = 0.0;
        msg.ki_position[i] = 0.0;
        msg.kd_position[i] = 0.0;
        msg.kp_velocity[i] = 0.0;
        msg.i_effort_min[i] = 0.0;
        msg.i_effort_max[i] = 0.0;
        msg.effort[i] = 0.0;
				msg.k_effort[i] = (byte)255; // take complete control of joints (remove BDI control)
      }
    }
    
    public int dim()
    {
    	if (mode==1 || mode==2)
    		return m_num_joints;
    	else if (mode==3)
    		return 2*m_num_joints;
    	else if (mode==4)
    		return 3*m_num_joints;
    	else
    		return -1;
    }
    
    public drake.util.CoordinateFrameData decode(byte[] data)
    {
      try {
        drc.atlas_command_t msg = new drc.atlas_command_t(data);
        return decode(msg);
      } catch (IOException ex) {
        System.out.println("Exception: " + ex);
      }
      return null;
    }

    public drake.util.CoordinateFrameData decode(drc.atlas_command_t msg)
    { 
      drake.util.CoordinateFrameData fdata = new drake.util.CoordinateFrameData();

      switch(mode) {
      case 1:
      case 2:
        fdata.val = new double[m_num_joints];
        break;                
      case 3:
        fdata.val = new double[2*m_num_joints];
        break;        
      case 4:
        fdata.val = new double[3*m_num_joints];
        break;        
      default:
        throw new IllegalStateException("Unknown mode: " + mode);    
      }

      fdata.t = (double)msg.utime / 1000000.0;

      int j;
      for (int i=0; i<m_num_joints; i++) {
        j = drake_to_atlas_joint_map[i];
        switch(mode) {
        case 1:
          fdata.val[i] = msg.effort[j];
          //cycle through gains, checking for any non-zero
//          if (msg.kp_position[j] != 0 || msg.ki_position[j] != 0 || msg.kd_position[j] != 0 || msg.kp_velocity[j] != 0) {
//            throw new IllegalArgumentException("Encoder is set to mode 1, but message has non-zero gains");
//          }
          break;
        case 2:
          fdata.val[i] = msg.position[j];
          break;
        case 3:
          fdata.val[i] = msg.position[j];
          fdata.val[m_num_joints+i] = msg.velocity[j];
          break;
        case 4:
          fdata.val[i] = msg.position[j];
          fdata.val[m_num_joints+i] = msg.velocity[j];
        	fdata.val[2*m_num_joints+i] = msg.effort[j];
          break;
        }
      }
      return fdata;
    }

    public LCMEncodable encode(drake.util.CoordinateFrameData d)
    {
      msg.utime = (long)(d.t*1000000);
      int j;
      for (int i=0; i<m_num_joints; i++) {
        j = drake_to_atlas_joint_map[i];
        if (mode==1) {
          msg.effort[j] = d.val[i];
        } else if (mode==2) {
          msg.position[j] = d.val[i];
        } else if (mode==3) {
          msg.position[j] = d.val[i];
          msg.velocity[j] = d.val[m_num_joints+i];
        } else if (mode==4) {
        	msg.position[j] = d.val[i];
        	msg.velocity[j] = d.val[m_num_joints+i];
        	msg.effort[j] = d.val[2*m_num_joints+i];
        }        
      }
      return msg;
    }
    
    public String timestampName()
    {
      return "utime";
    }
}
