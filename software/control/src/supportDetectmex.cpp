#include "ControlUtil.h"

using namespace std;

struct SupportDetectData {
  RigidBodyManipulator* r;
  void* map_ptr;
};

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  int error;
  if (nrhs<1) mexErrMsgTxt("usage: ptr = supportDetectmex(0,robot_obj,...); alpha=supportDetectmex(ptr,...,...)");
  if (nlhs<1) mexErrMsgTxt("take at least one output... please.");
  
  struct SupportDetectData* pdata;
//   mxArray* pm;
  double* pr;
  int i,j;

  if (mxGetScalar(prhs[0])==0) { // then construct the data object and return
    pdata = new struct SupportDetectData;
    
    // get robot mex model ptr
    if (!mxIsNumeric(prhs[1]) || mxGetNumberOfElements(prhs[1])!=1)
      mexErrMsgIdAndTxt("DRC:supportDetectmex:BadInputs","the second argument should be the robot mex ptr");
    memcpy(&(pdata->r),mxGetData(prhs[1]),sizeof(pdata->r));
        
     // get the map ptr back from matlab
     if (!mxIsNumeric(prhs[2]) || mxGetNumberOfElements(prhs[2])!=1)
     mexErrMsgIdAndTxt("DRC:supportDetectmex:BadInputs","the third argument should be the map ptr");
     memcpy(&(pdata->map_ptr),mxGetPr(prhs[2]),sizeof(pdata->map_ptr));
    
    if (!pdata->map_ptr)
      mexWarnMsgTxt("Map ptr is NULL.  Assuming flat terrain at z=0");
   
     
    mxClassID cid;
    if (sizeof(pdata)==4) cid = mxUINT32_CLASS;
    else if (sizeof(pdata)==8) cid = mxUINT64_CLASS;
    else mexErrMsgIdAndTxt("Drake:supportDetectmex:PointerSize","Are you on a 32-bit machine or 64-bit machine??");
     
    plhs[0] = mxCreateNumericMatrix(1,1,cid,mxREAL);
    memcpy(mxGetData(plhs[0]),&pdata,sizeof(pdata));
     
    return;
  }
  
  // first get the ptr back from matlab
  if (!mxIsNumeric(prhs[0]) || mxGetNumberOfElements(prhs[0])!=1)
    mexErrMsgIdAndTxt("DRC:supportDetectmex:BadInputs","the first argument should be the ptr");
  memcpy(&pdata,mxGetData(prhs[0]),sizeof(pdata));

  int nq = pdata->r->num_dof;

  int narg=1;  
  double *q = mxGetPr(prhs[narg++]);
  double *qd = &q[nq];
  
  int desired_support_argid = narg++;

  double* double_contact_sensor = mxGetPr(prhs[narg]); int len = mxGetNumberOfElements(prhs[narg++]);
  VectorXi contact_sensor(len);  
  for (i=0; i<len; i++)
    contact_sensor(i)=(int)double_contact_sensor[i];
  double contact_threshold = mxGetScalar(prhs[narg++]);
  double terrain_height = mxGetScalar(prhs[narg++]); // nonzero if we're using DRCFlatTerrainMap
  
  int contact_logic_AND = (int) mxGetScalar(prhs[narg++]); // true if we should AND plan and sensor, false if we should OR them

  pdata->r->doKinematics(q,false,qd);

  //---------------------------------------------------------------------
  // Compute active support from desired supports -----------------------

  vector<SupportStateElement> active_supports;
  set<int> contact_bodies; // redundant, clean up later
  int num_active_contact_pts=0;
  if (!mxIsEmpty(prhs[desired_support_argid])) {
    VectorXd phi;
    mxArray* mxBodies = mxGetProperty(prhs[desired_support_argid],0,"bodies");
    if (!mxBodies) mexErrMsgTxt("couldn't get bodies");
    double* pBodies = mxGetPr(mxBodies);
    mxArray* mxContactPts = mxGetProperty(prhs[desired_support_argid],0,"contact_pts");
    if (!mxContactPts) mexErrMsgTxt("couldn't get contact points");
    mxArray* mxContactSurfaces = mxGetProperty(prhs[desired_support_argid],0,"contact_surfaces");
    if (!mxContactSurfaces) mexErrMsgTxt("couldn't get contact surfaces");
    double* pContactSurfaces = mxGetPr(mxContactSurfaces);
    
    for (i=0; i<mxGetNumberOfElements(mxBodies);i++) {
      mxArray* mxBodyContactPts = mxGetCell(mxContactPts,i);
      int nc = mxGetNumberOfElements(mxBodyContactPts);
      if (nc<1) continue;
      
      SupportStateElement se;
      se.body_idx = (int) pBodies[i]-1;
      pr = mxGetPr(mxBodyContactPts); 
      for (j=0; j<nc; j++) {
        se.contact_pt_inds.insert((int)pr[j]-1);
      }
      se.contact_surface = (int) pContactSurfaces[i]-1;
      
      if (contact_threshold == -1) { // ignore terrain
        if (contact_sensor(i)!=0) { // no sensor info, or sensor says yes contact
          active_supports.push_back(se);
          num_active_contact_pts += nc;
          contact_bodies.insert((int)se.body_idx); 
        }
      } 
      else {
        contactPhi(pdata->r,se,pdata->map_ptr,phi,terrain_height);
        bool in_contact = true;
        if (contact_logic_AND) {
          in_contact =  (phi.minCoeff()<=contact_threshold || contact_sensor(i)==1); // any contact below threshold (kinematically) OR contact sensor says yes contact
        }

        if (in_contact) { 
          active_supports.push_back(se);
          num_active_contact_pts += nc;
          contact_bodies.insert((int)se.body_idx);
        }
      }
    }
  }

  if (nlhs>0) {
    plhs[0] = mxCreateDoubleMatrix(1,active_supports.size(),mxREAL);
    pr = mxGetPr(plhs[0]);
    int i=0;
    for (vector<SupportStateElement>::iterator iter = active_supports.begin(); iter!=active_supports.end(); iter++) {
      pr[i++] = (double) (iter->body_idx + 1);
    }
  }
} 