# This awkward line exists to support zsh (instead of bash). 
# Due to limitations of zsh, it must be the first line in the file. Sorry.
thisFile=$_
if [ $BASH ] 
then
  # may be a relative or absolute path
  thisFile=${BASH_SOURCE[0]}
fi

# This function sets the DRC_BASE environment variable to be the DRC
# root directory.  The path to the root directory is computed using
# the absolute path of this bash file.  Due to symlinks, this bash
# file can be located in $DRC_BASE/software/config or
# $DRC_BASE/software/build/config.
set_drc_base()
{
  # use cd and pwd to get an absolute path
  configParentDir="$(cd "$(dirname "$thisFile")/.." && pwd)"

  # different cases for software/config or software/build/config
  case "$(basename $configParentDir)" in
    "software") export DRC_BASE=$(dirname $configParentDir);;
    "build") export DRC_BASE=$(dirname $(dirname $configParentDir));;
    *) echo "Warning: DRC environment file is stored in unrecognized location: $thisFile";;
  esac
}

setup_drcsim()
{
  drcsim_setup_file=/usr/local/share/drcsim/setup.sh
  if [ -f $drcsim_setup_file ]; then
   source $drcsim_setup_file # this will also source setup.sh for ROS and Gazebo
  fi

  export ROS_PACKAGE_PATH=$DRC_BASE/ros_workspace:$ROS_PACKAGE_PATH
  export GAZEBO_PLUGIN_PATH=$DRC_BASE/software/build/lib:$GAZEBO_PLUGIN_PATH
  export GAZEBO_PLUGIN_PATH=$DRC_BASE/ros_workspace/mit_drcsim_plugins/lib:$GAZEBO_PLUGIN_PATH
  export GAZEBO_MODEL_PATH=$DRC_BASE/software/models/mit_gazebo_models:$GAZEBO_MODEL_PATH
  export GAZEBO_MODEL_PATH=$DRC_BASE/software/models/mit_gazebo_objects:$GAZEBO_MODEL_PATH
  export VRC_CHEATS_ENABLED=1
}


setup_drc()
{
  export PATH=$PATH:$DRC_BASE/software/build/bin
  export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
  export LD_LIBRARY_PATH=$DRC_BASE/software/build/lib:$DRC_BASE/software/build/lib64:$LD_LIBRARY_PATH
  export CLASSPATH=$CLASSPATH:/usr/local/share/java/lcm.jar:$DRC_BASE/software/build/share/java/lcmtypes_drc_lcmtypes.jar
  export CLASSPATH=$CLASSPATH:$DRC_BASE/software/build/share/java/drake.jar
  export PKG_CONFIG_PATH=$DRC_BASE/software/build/lib/pkgconfig:$DRC_BASE/software/build/lib64/pkgconfig:$PKG_CONFIG_PATH

  # gurobi
  export GUROBI_HOME=$DRC_BASE/software/externals/gurobi/gurobi562/linux64
  export PATH=$PATH:$GUROBI_HOME/bin
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$GUROBI_HOME/lib
  export GRB_LICENSE_FILE=$HOME/gurobi.lic

  # python path
  export PYTHONPATH=$PYTHONPATH:$DRC_BASE/software/build/lib/python2.7/site-packages:$DRC_BASE/software/build/lib/python2.7/dist-packages

  # enable some warnings by default
  export CXXFLAGS="$CXXFLAGS -Wreturn-type -Wuninitialized"
  export CFLAGS="$CFLAGS -Wreturn-type -Wuninitialized"

  # required by Atlas API runtime
  export ATLAS_ROBOT_INTERFACE=$DRC_BASE/software/drivers/atlas/AtlasRobotInterface_3.0.0
}

setup_robot_computers()
{
  if [[ "paladin-04|paladin-05|paladin-06|paladin-10|paladin-12" =~ $(hostname) ]]
  then
      export LCM_DEFAULT_URL=${LCM_URL_DRC_RADIO}
  fi

  if [ "atlas0" = $(hostname) ]
  then
      export LCM_DEFAULT_URL=${LCM_URL_DRC_PERCEPTION}
  elif [ "atlas1" = $(hostname) ]
  then
      export LCM_DEFAULT_URL=${LCM_URL_DRC_PERCEPTION}
  elif [ "atlas2" = $(hostname) ]
  then
      export LCM_DEFAULT_URL=${LCM_URL_DRC_CONTROL}
  fi
}

setup_network_sim()
{
    export LCM_URL_DRC_ROBOT="udpm://239.255.76.68:7668?ttl=0"
    export LCM_URL_DRC_BASE="udpm://239.255.76.67:7667?ttl=0" 
}

setup_lcm_communities()
{
    export LCM_URL_DRC_DEFAULT="udpm://239.255.76.67:7667?ttl=1"
    if [ "true" = ${DEBUG_NETWORK} ]
    then
	export LCM_URL_DRC_RADIO=${LCM_URL_DRC_DEFAULT}
	export LCM_URL_DRC_CONTROL=${LCM_URL_DRC_DEFAULT}
	export LCM_URL_DRC_PERCEPTION=${LCM_URL_DRC_DEFAULT}
	export LCM_URL_DRC_ATLAS_0_2=${LCM_URL_DRC_DEFAULT}
	export LCM_URL_DRC_ATLAS_1_2=${LCM_URL_DRC_DEFAULT}
    else
	export LCM_URL_DRC_RADIO="udpm://239.255.76.70:7670?ttl=1"
	export LCM_URL_DRC_CONTROL="udpm://239.255.76.80:7680?ttl=1"
	export LCM_URL_DRC_PERCEPTION="udpm://239.255.76.81:7681?ttl=1"
	export LCM_URL_DRC_ATLAS_0_2="udpm://239.255.76.82:7682?ttl=1"
	export LCM_URL_DRC_ATLAS_1_2="udpm://239.255.76.83:7683?ttl=1"
    fi
}

export DEBUG_NETWORK="true"

set_drc_base
#setup_drcsim
setup_drc
setup_network_sim
setup_lcm_communities
setup_robot_computers

# aliases
alias cddrc='cd $DRC_BASE/software'
alias rundrc='bot-procman-sheriff -l $DRC_BASE/software/config/drc_robot.pmd'
