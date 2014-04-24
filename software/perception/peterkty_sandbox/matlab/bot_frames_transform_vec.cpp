#include <lcm/lcm.h>
#include <bot_param/param_client.h>
#include <bot_frames/bot_frames.h>
#include <math.h>
#include <stdio.h>

// input args: srcframe, dstframe , src coordinate
// output args: dst coordinate
int main()
{
    
   lcm_t *lcm;
   BotParam * param;
   BotFrames * frames;
   
   lcm = bot_lcm_get_global (NULL);
   if(!lcm)
     printf("Error getting LCM\n");
   param = bot_param_new_from_server (lcm, 0);
   if(!param)
     printf("Error getting param\n");
   frames = bot_frames_new (lcm, param);
   if(!frames)
     printf("Error getting frames\n");
   /*
   char* src = mxArrayToString(prhs[0]);
   char* dst = mxArrayToString(prhs[1]);
   double* xyz_src = mxGetPr(prhs[3]);
   
   
   //mxCreateNumericMatrix(m, n, classid,   ComplexFlag)
   plhs[0] = mxCreateNumericMatrix(3, 1, mxDOUBLE_CLASS, mxREAL); 
   double* xyz_dst = (double*)mxGetData(plhs[0]);
   bot_frames_transform_vec (frames, src, dst, xyz_src, xyz_dst);

   mxFree(src);
   mxFree(dst);*/
   
    
}

