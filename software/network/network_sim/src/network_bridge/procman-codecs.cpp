#include "procman-codecs.h"


std::map<std::string, State<bot_procman::orders2_t, drc::PMDOrdersDiff> > PMDOrdersCodec::host_info_;
std::map<std::string, State<bot_procman::info2_t, drc::PMDInfoDiff> > PMDInfoCodec::host_info_;

using goby::glog;
using namespace goby::common::logger;

// new bot_procman_command2_t

bool make_cmd_diff(const bot_procman::command2_t& cmd, const bot_procman::command2_t* ref_cmd, drc::PMDCommandDiff* diff_cmd)
{
    bool empty = true;
    bool has_ref_cmd = (ref_cmd != 0);    
    if(!has_ref_cmd || cmd.exec_str != ref_cmd->exec_str)
    {
        diff_cmd->set_name(cmd.exec_str);
        empty = false;            
    }
    
    if(!has_ref_cmd || cmd.command_name != ref_cmd->command_name)
    {
        diff_cmd->set_nickname(cmd.command_name);
        empty = false;            
    }
    if(!has_ref_cmd || cmd.group != ref_cmd->group)
    {
        diff_cmd->set_group(cmd.group);
        empty = false;            
    }
    if(!has_ref_cmd || cmd.auto_respawn != ref_cmd->auto_respawn)
    {
        diff_cmd->set_auto_respawn(cmd.auto_respawn);
        empty = false;            
    }

    if(!has_ref_cmd || cmd.stop_signal != ref_cmd->stop_signal)
    {
        diff_cmd->set_stop_signal(cmd.stop_signal);
        empty = false;            
    }
    if(!has_ref_cmd || cmd.stop_time_allowed != ref_cmd->stop_time_allowed)
    {
        diff_cmd->set_stop_time_allowed(cmd.stop_time_allowed);
        empty = false;            
    }

    return empty;
}


bool reverse_cmd_diff(bot_procman::command2_t* cmd, const bot_procman::command2_t* ref_cmd, const drc::PMDCommandDiff& diff_cmd)
{
    bool has_ref_cmd = (ref_cmd != 0);
    
    cmd->exec_str = (!has_ref_cmd || diff_cmd.has_name()) ? diff_cmd.name() : ref_cmd->exec_str;
    cmd->command_name = (!has_ref_cmd || diff_cmd.has_nickname()) ? diff_cmd.nickname() : ref_cmd->command_name;
    cmd->group = (!has_ref_cmd || diff_cmd.has_group()) ? diff_cmd.group() : ref_cmd->group;
    cmd->auto_respawn = (!has_ref_cmd || diff_cmd.has_auto_respawn()) ? diff_cmd.auto_respawn() : ref_cmd->auto_respawn;
    cmd->stop_signal = (!has_ref_cmd || diff_cmd.has_stop_signal()) ? diff_cmd.stop_signal() : ref_cmd->stop_signal;
    cmd->stop_time_allowed = (!has_ref_cmd || diff_cmd.has_stop_time_allowed()) ? diff_cmd.stop_time_allowed() : ref_cmd->stop_time_allowed;

    cmd->num_options = 0;
    return true;
}


// PMD_ORDERS

bool PMDOrdersCodec::make_diff(const bot_procman::orders2_t& orders, const bot_procman::orders2_t& reference, drc::PMDOrdersDiff* diff)
{   
    // modulus to tenth of seconds, so full messages can be resolved to ~25 seconds
    diff->set_reference_time((reference.utime / 100000) % 256);
    
    diff->set_utime(orders.utime - reference.utime);

    if(orders.sheriff_name != reference.sheriff_name)
        diff->set_sheriff_name(orders.sheriff_name);    

    for(int i = 0, n = orders.cmds.size(); i < n; ++i)
    {
        const bot_procman::sheriff_cmd2_t& cmd = orders.cmds[i];

        drc::PMDOrdersDiff::PMDSheriffCmdDiff* diff_cmd = diff->add_cmds();
        
        bool has_ref_cmd = i < static_cast<int>(reference.cmds.size());
        
        const bot_procman::sheriff_cmd2_t* ref_cmd = has_ref_cmd ? &reference.cmds[i] : 0;
        
        bool empty = make_cmd_diff(cmd.cmd, (ref_cmd ? &ref_cmd->cmd : 0), diff_cmd->mutable_cmd());
        
        if(!has_ref_cmd || cmd.desired_runid != ref_cmd->desired_runid)
        {
            diff_cmd->set_desired_runid(cmd.desired_runid);
            empty = false;
        }
        
        if(!has_ref_cmd || cmd.force_quit != ref_cmd->force_quit)
        {
            diff_cmd->set_force_quit(cmd.force_quit);
            empty = false;
        }
        
        if(!has_ref_cmd || cmd.sheriff_id != ref_cmd->sheriff_id)
        {
            diff_cmd->set_sheriff_id(cmd.sheriff_id);
            empty = false;            
        }
        
        if(!empty)
            diff_cmd->set_index(i);
        else
            diff->mutable_cmds()->RemoveLast();
    }
            
    glog.is(VERBOSE) && glog << "Made PMD_ORDERS diff: " << pb_to_short_string(*diff,true) << std::endl;
    return true;
}

bool PMDOrdersCodec::reverse_diff(bot_procman::orders2_t* orders, const bot_procman::orders2_t& reference, const drc::PMDOrdersDiff& diff)
{
    glog.is(VERBOSE) && glog << "Received PMD_ORDERS diff: " << pb_to_short_string(diff,true) << std::endl;

    
    if(((reference.utime / 100000) % 256) != diff.reference_time())
    {
        glog.is(WARN) && glog << "Wrong time reference, cannot reassemble diff. " << std::endl;
        return false;
    }
    
    orders->utime = reference.utime + diff.utime();
    orders->host = reference.host;
    orders->sheriff_name = diff.has_sheriff_name() ? diff.sheriff_name() : reference.sheriff_name;
    orders->ncmds = diff.cmds_size() > 0 ? std::max(reference.ncmds, diff.cmds(diff.cmds_size()-1).index()) : reference.ncmds;

    int j = 0;
    for(int i = 0, n = orders->ncmds; i < n; ++i)
    {
        bot_procman::sheriff_cmd2_t cmd;

        if(j < diff.cmds_size() && diff.cmds(j).index() < i)
            ++j;
        
        drc::PMDOrdersDiff::PMDSheriffCmdDiff diff_cmd = (j < diff.cmds_size() && diff.cmds(j).index() == i) ?
            diff.cmds(j) : drc::PMDOrdersDiff::PMDSheriffCmdDiff();
        // 
        bool has_ref_cmd = i < static_cast<int>(reference.cmds.size());
        
        const bot_procman::sheriff_cmd2_t* ref_cmd = has_ref_cmd ? &reference.cmds[i] : 0;

        reverse_cmd_diff(&cmd.cmd, (ref_cmd ? &ref_cmd->cmd : 0), diff_cmd.cmd());
        
        cmd.desired_runid = (!has_ref_cmd || diff_cmd.has_desired_runid()) ? diff_cmd.desired_runid() : ref_cmd->desired_runid;
        cmd.force_quit = (!has_ref_cmd || diff_cmd.has_force_quit()) ? diff_cmd.force_quit() : ref_cmd->force_quit;
        cmd.sheriff_id = (!has_ref_cmd || diff_cmd.has_sheriff_id()) ? diff_cmd.sheriff_id() : ref_cmd->sheriff_id;
        
        orders->cmds.push_back(cmd);
    }
    
    orders->num_options = 0;
    
    
    return true;
}

// PMD_INFO

bool PMDInfoCodec::make_diff(const bot_procman::info2_t& info, const bot_procman::info2_t& reference, drc::PMDInfoDiff* diff)
{
    
    diff->set_reference_time((reference.utime / 100000) % 256);

    diff->set_utime(info.utime - reference.utime);

    for(int i = 0, n = info.cmds.size(); i < n; ++i)
    {
        const bot_procman::deputy_cmd2_t& cmd = info.cmds[i];

        drc::PMDInfoDiff::PMDDeputyCmdDiff* diff_cmd = diff->add_cmds();
        
        bool has_ref_cmd = i < static_cast<int>(reference.cmds.size());
        
        const bot_procman::deputy_cmd2_t* ref_cmd = has_ref_cmd ? &reference.cmds[i] : 0;

        bool empty = make_cmd_diff(cmd.cmd, (ref_cmd ? &ref_cmd->cmd : 0), diff_cmd->mutable_cmd());

        if(!has_ref_cmd || cmd.pid != ref_cmd->pid)
        {
            diff_cmd->set_pid(cmd.pid);
            empty = false;            
        }
        if(!has_ref_cmd || cmd.actual_runid != ref_cmd->actual_runid)
        {
            diff_cmd->set_actual_runid(cmd.actual_runid);
            empty = false;            
        }
        if(!has_ref_cmd || cmd.exit_code != ref_cmd->exit_code)
        {
            diff_cmd->set_exit_code(cmd.exit_code);
            empty = false;            
        }
        if(!has_ref_cmd || cmd.sheriff_id != ref_cmd->sheriff_id)
        {
            diff_cmd->set_sheriff_id(cmd.sheriff_id);
            empty = false;            
        }
        
        if(!empty)
            diff_cmd->set_index(i);
        else
            diff->mutable_cmds()->RemoveLast();
        
        
    }       
    glog.is(VERBOSE) && glog << "Made PMD_INFO diff: " << pb_to_short_string(*diff,true) << std::endl;
    
    return true;
}

bool PMDInfoCodec::reverse_diff(bot_procman::info2_t* info, const bot_procman::info2_t& reference, const drc::PMDInfoDiff& diff)
{
    glog.is(VERBOSE) && glog << "Received PMD_INFO diff at time: " << reference.utime + diff.utime() << "  : " << pb_to_short_string(diff,true) << std::endl;


    if(((reference.utime / 100000) % 256) != diff.reference_time())
        return false;
    
    info->utime = reference.utime + diff.utime();
    info->host = reference.host;    

    info->cpu_load = std::numeric_limits<float>::quiet_NaN();
    info->phys_mem_total_bytes = -1;
    info->phys_mem_free_bytes = -1;
    info->swap_total_bytes = -1;
    info->swap_free_bytes = -1;
    info->num_options = 0;
    
    info->ncmds = diff.cmds_size() > 0 ? std::max(reference.ncmds, diff.cmds(diff.cmds_size()-1).index()) : reference.ncmds;

    int j = 0;
    for(int i = 0, n = info->ncmds; i < n; ++i)
    {
        bot_procman::deputy_cmd2_t cmd;

        if(j < diff.cmds_size() && diff.cmds(j).index() < i)
            ++j;
        
        drc::PMDInfoDiff::PMDDeputyCmdDiff diff_cmd = (j < diff.cmds_size() && diff.cmds(j).index() == i) ?
            diff.cmds(j) : drc::PMDInfoDiff::PMDDeputyCmdDiff();
        // 

        bool has_ref_cmd = i < static_cast<int>(reference.cmds.size());
        
        const bot_procman::deputy_cmd2_t* ref_cmd = has_ref_cmd ? &reference.cmds[i] : 0;
        reverse_cmd_diff(&cmd.cmd, (ref_cmd ? &ref_cmd->cmd : 0), diff_cmd.cmd());

        cmd.pid = (!has_ref_cmd || diff_cmd.has_pid()) ? diff_cmd.pid() : ref_cmd->pid;
        cmd.actual_runid = (!has_ref_cmd || diff_cmd.has_actual_runid()) ? diff_cmd.actual_runid() : ref_cmd->actual_runid;
        cmd.exit_code = (!has_ref_cmd || diff_cmd.has_exit_code()) ? diff_cmd.exit_code() : ref_cmd->exit_code;
        cmd.sheriff_id = (!has_ref_cmd || diff_cmd.has_sheriff_id()) ? diff_cmd.sheriff_id() : ref_cmd->sheriff_id;

        cmd.cpu_usage = std::numeric_limits<float>::quiet_NaN();
        cmd.mem_vsize_bytes = -1;
        cmd.mem_rss_bytes = -1;        
        
        info->cmds.push_back(cmd);
    }

    return true;
}


    

