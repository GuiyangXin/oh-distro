/* 
 * A c++ version of (significant pieces of) the QPController.m mimoOutput method. 
 *
 * Todo:
 *   handle the no supports case (arguments into mex starting with B_ls will currently fail)
 *   use fixed-size matrices (or at least pre-allocated)
 *       for instance: #define nq 
 *       set MaxRowsAtCompileTime (http://eigen.tuxfamily.org/dox/TutorialMatrixClass.html)
 *   some matrices might be better off using RowMajor
 */



#include <mex.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <set>
#include <vector>
#include <Eigen/Dense>
//#define EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
//#include <Eigen/Sparse>

#ifdef USE_MAPS

#include "mexmaps/MapLib.hpp"
#include <maps/ViewBase.hpp>

#endif

#include "fastQP.h"
#include "RigidBodyManipulator.h"

const int m_surface_tangents = 2;  // number of faces in the friction cone approx
//const double mu = 1.0;  // coefficient of friction
const double REG = 1e-8;

using namespace std;

struct QPControllerData {
  GRBenv *env;
  RigidBodyManipulator* r;
  RigidBodyManipulator* multi_robot;
  double w; // objective function weight
  double slack_limit; // maximum absolute magnitude of acceleration slack variable values
//  int Rnnz,*Rind1,*Rind2; double* Rval; // my sparse representation of R_con - the quadratic cost on input
  MatrixXd Rdiag, Rdiaginv;  // these are really vectors, but I need to pass MatrixXd* to the block diagonal solvers
  MatrixXd B, B_con, B_free;
  set<int> free_dof, con_dof, free_inputs, con_inputs; 
  VectorXd umin_con, umax_con;
  ArrayXd umin,umax;
  void* map_ptr;
  
  // preallocate memory
  MatrixXd H;
  VectorXd C;
  MatrixXd J, Jdot;
  MatrixXd H_con, H_free;
  VectorXd C_con, C_free;
  VectorXd qdd_free;
  VectorXd q_ddot_des_free;
  VectorXd qd_con;
  MatrixXd Hqp_free;
  RowVectorXd fqp_free;
  MatrixXd J_free, Jdot_free;
  VectorXd qd_free;
  VectorXd q_ddot_des_con;
  MatrixXd Hqp_con;
  RowVectorXd fqp_con;
  MatrixXd J_con, Jdot_con;
//  MatrixXd Ain_lb_ub, bin_lb_ub;
};

// helper function for shuffling debugging data back into matlab
template <int Rows, int Cols>
mxArray* eigenToMatlab(Matrix<double,Rows,Cols> &m)
{
  mxArray* pm = mxCreateDoubleMatrix(m.rows(),m.cols(),mxREAL);
  if (m.rows()*m.cols()>0)
  	memcpy(mxGetPr(pm),m.data(),sizeof(double)*m.rows()*m.cols());
  return pm;
}



mxArray* myGetProperty(const mxArray* pobj, const char* propname)
{
  mxArray* pm = mxGetProperty(pobj,0,propname);
  if (!pm) mexErrMsgIdAndTxt("DRC:QPControllermex:BadInput","QPControllermex is trying to load object property '%s', but failed.", propname);
  return pm;
}

mxArray* myGetField(const mxArray* pobj, const char* propname)
{
  mxArray* pm = mxGetField(pobj,0,propname);
  if (!pm) mexErrMsgIdAndTxt("DRC:QPControllermex:BadInput","QPControllermex is trying to load object field '%s', but failed.", propname);
  return pm;
}

typedef struct _support_state_element
{
  int body_idx;
  set<int> contact_pt_inds;
} SupportStateElement;


template <typename DerivedA, typename DerivedB>
void getRows(set<int> &rows, MatrixBase<DerivedA> const &M, MatrixBase<DerivedB> &Msub)
{
  if (rows.size()==M.rows()) {
    Msub = M; 
    return;
  }
  
  int i=0;
  for (set<int>::iterator iter=rows.begin(); iter!=rows.end(); iter++)
    Msub.row(i++) = M.row(*iter);
}

template <typename DerivedA, typename DerivedB>
void getCols(set<int> &cols, MatrixBase<DerivedA> const &M, MatrixBase<DerivedB> &Msub)
{
  if (cols.size()==M.cols()) {
    Msub = M;
    return;
  }
  int i=0;
  for (set<int>::iterator iter=cols.begin(); iter!=cols.end(); iter++)
    Msub.col(i++) = M.col(*iter);
}

void collisionDetect(void* map_ptr, Vector3d const & contact_pos, Vector3d &pos, Vector3d *normal, double terrain_height)
{
  if (map_ptr) {
#ifdef USE_MAPS    
    Vector3f floatPos, floatNormal;
    auto state = static_cast<mexmaps::MapHandle*>(map_ptr);
    if (state != NULL) {
      auto view = state->getView();
      if (view != NULL) {
        if (view->getClosest(contact_pos.cast<float>(),floatPos,floatNormal)) {
          pos = floatPos.cast<double>();
          if (normal) *normal = floatNormal.cast<double>();
          return;
        }
      }
    }
#endif      
  } else {
//    mexPrintf("Warning: using 0,0,1 as normal\n");
    pos << contact_pos.topRows(2), terrain_height;
    if (normal) *normal << 0,0,1;
  }
  // just assume mu = 1 for now
}

void surfaceTangents(const Vector3d & normal, Matrix<double,3,m_surface_tangents> & d)
{
  Vector3d t1,t2;
  double theta;
  
  if (1 - normal(2) < 10e-8) { // handle the unit-normal case (since it's unit length, just check z)
    t1 << 1,0,0;
  } else { // now the general case
    t1 << normal(2), -normal(1), 0; // normal.cross([0;0;1])
    t1 /= sqrt(normal(1)*normal(1) + normal(2)*normal(2));
  }
      
  t2 = t1.cross(normal);
      
  for (int k=0; k<m_surface_tangents; k++) {
    theta = k*M_PI/m_surface_tangents;
    d.col(k)=cos(theta)*t1 + sin(theta)*t2;
  }
}

int contactPhi(struct QPControllerData* pdata, SupportStateElement& supp, VectorXd &phi,double terrain_height)
{
  RigidBody* b = &(pdata->r->bodies[supp.body_idx]);
	int nc = supp.contact_pt_inds.size();
	phi.resize(nc);

	if (nc<1) return nc;

  Vector3d contact_pos,pos,normal; Vector4d tmp;

  int i=0;
  for (set<int>::iterator pt_iter=supp.contact_pt_inds.begin(); pt_iter!=supp.contact_pt_inds.end(); pt_iter++) {
  	if (*pt_iter<0 || *pt_iter>=b->contact_pts.cols()) mexErrMsgIdAndTxt("DRC:QPControllermex:BadInput","requesting contact pt %d but body only has %d pts",*pt_iter,b->contact_pts.cols());
		tmp = b->contact_pts.col(*pt_iter);
		pdata->r->forwardKin(supp.body_idx,tmp,0,contact_pos);
		collisionDetect(pdata->map_ptr,contact_pos,pos,NULL,terrain_height);

		pos -= contact_pos;  // now -rel_pos in matlab version
		phi(i) = pos.norm();
		if (pos.dot(normal)>0)
			phi(i)=-phi(i);
		i++;
  }
	return nc;
}

int contactConstraints(struct QPControllerData* pdata, int nc, vector<SupportStateElement>& supp, MatrixXd &n, MatrixXd &D, MatrixXd &Jp, MatrixXd &Jpdot,double terrain_height)
{
  int i, j, k=0, nq = pdata->r->num_dof;

//  phi.resize(nc);
  n.resize(nc,nq);
  D.resize(nq,nc*2*m_surface_tangents);
  Jp.resize(3*nc,nq);
  Jpdot.resize(3*nc,nq);
  
  Vector3d contact_pos,pos,normal; Vector4d tmp;
  MatrixXd J(3,nq);
  Matrix<double,3,m_surface_tangents> d;
  
  for (vector<SupportStateElement>::iterator iter = supp.begin(); iter!=supp.end(); iter++) {
    RigidBody* b = &(pdata->r->bodies[iter->body_idx]);
    if (nc>0) {
      for (set<int>::iterator pt_iter=iter->contact_pt_inds.begin(); pt_iter!=iter->contact_pt_inds.end(); pt_iter++) {
      	int pt_iter_val = *pt_iter;
      	if (*pt_iter<0 || *pt_iter>=b->contact_pts.cols()) mexErrMsgIdAndTxt("DRC:QPControllermex:BadInput","requesting contact pt %d but body only has %d pts",*pt_iter,b->contact_pts.cols());
        tmp = b->contact_pts.col(*pt_iter);
        pdata->r->forwardKin(iter->body_idx,tmp,0,contact_pos);
        pdata->r->forwardJac(iter->body_idx,tmp,0,J);

        collisionDetect(pdata->map_ptr,contact_pos,pos,&normal,terrain_height);
        
// phi not being used right now
//        pos -= contact_pos;  // now -rel_pos in matlab version
//        phi(k) = pos.norm();
//        if (pos.dot(normal)>0) phi(k)=-phi(k);

        surfaceTangents(normal,d);

        n.row(k) = normal.transpose()*J;
        for (j=0; j<m_surface_tangents; j++) {
          D.col(2*k*m_surface_tangents+j) = J.transpose()*d.col(j);
          D.col((2*k+1)*m_surface_tangents+j) = -D.col(2*k*m_surface_tangents+j);
        }

        // store away kin sols into Jp and Jpdot
        // NOTE: I'm cheating and using a slightly different ordering of J and Jdot here
        Jp.block(3*k,0,3,nq) = J;
        pdata->r->forwardJacDot(iter->body_idx,tmp,J);
        Jpdot.block(3*k,0,3,nq) = J;
        
        k++;
      }
    }
  }
  
  return k;
}


void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  int error;
  if (nrhs<1) mexErrMsgTxt("usage: ptr = QPControllermex(0,control_obj,robot_obj,...); alpha=QPControllermex(ptr,...,...)");
  if (nlhs<1) mexErrMsgTxt("take at least one output... please.");
  
  struct QPControllerData* pdata;
  mxArray* pm;
  double* pr;
  int i,j;

  if (mxGetScalar(prhs[0])==0) { // then construct the data object and return
    pdata = new struct QPControllerData;
    
    // get control object properties
    const mxArray* pobj = prhs[1];
    
    pm= myGetProperty(pobj,"w");
    pdata->w = mxGetScalar(pm);
    
    pm = myGetProperty(pobj,"slack_limit");
    pdata->slack_limit = mxGetScalar(pm);

    pm = myGetProperty(pobj,"free_dof");
    pr = mxGetPr(pm);
    for (i=0; i<mxGetNumberOfElements(pm); i++)
      pdata->free_dof.insert((int)pr[i] - 1);
    
    pm = myGetProperty(pobj,"con_dof");
    pr = mxGetPr(pm);
    for (i=0; i<mxGetNumberOfElements(pm); i++)
      pdata->con_dof.insert((int)pr[i] - 1);
    int nq_con = pdata->con_dof.size();

    pm = myGetProperty(pobj,"free_inputs");
    pr = mxGetPr(pm);
    for (i=0; i<mxGetNumberOfElements(pm); i++)
      pdata->free_inputs.insert((int)pr[i] - 1);

    pm = myGetProperty(pobj,"con_inputs");
    pr = mxGetPr(pm);
    for (i=0; i<mxGetNumberOfElements(pm); i++)
      pdata->con_inputs.insert((int)pr[i] - 1);
    int nu_con = pdata->con_inputs.size();
    
    // get robot mex model ptr
    if (!mxIsNumeric(prhs[2]) || mxGetNumberOfElements(prhs[2])!=1)
      mexErrMsgIdAndTxt("DRC:QPControllermex:BadInputs","the third argument should be the robot mex ptr");
    memcpy(&(pdata->r),mxGetData(prhs[2]),sizeof(pdata->r));
    
    pdata->B.resize(mxGetM(prhs[3]),mxGetN(prhs[3]));
    memcpy(pdata->B.data(),mxGetPr(prhs[3]),sizeof(double)*mxGetM(prhs[3])*mxGetN(prhs[3]));

    int nq = pdata->r->num_dof, nu = pdata->B.cols();
    
    pdata->umin.resize(nu);
    pdata->umax.resize(nu);
    memcpy(pdata->umin.data(),mxGetPr(prhs[4]),sizeof(double)*nu);
    memcpy(pdata->umax.data(),mxGetPr(prhs[5]),sizeof(double)*nu);
    pdata->umin_con.resize(nu_con);
    pdata->umax_con.resize(nu_con);
    getRows(pdata->con_inputs,pdata->umin.matrix(),pdata->umin_con);
    getRows(pdata->con_inputs,pdata->umax.matrix(),pdata->umax_con);

    {
      pdata->B_con.resize(pdata->con_dof.size(),pdata->con_inputs.size());
      MatrixXd tmp(nq,pdata->con_inputs.size());
      getCols(pdata->con_inputs,pdata->B,tmp);
      getRows(pdata->con_dof,tmp,pdata->B_con);
    
      if (pdata->free_dof.size()>0 && pdata->free_inputs.size()>0) {
        pdata->B_free.resize(pdata->free_dof.size(),pdata->free_inputs.size());
        tmp.resize(nq,pdata->free_inputs.size());
        getCols(pdata->free_inputs,pdata->B,tmp);
        getRows(pdata->free_dof,tmp,pdata->B_free);
      }
    }

    {
      pm = myGetProperty(pobj,"Rdiag");
      pdata->Rdiag.resize(pdata->con_inputs.size(),1);
      Map<VectorXd> Rdiagfull(mxGetPr(pm),mxGetNumberOfElements(pm));
      getRows(pdata->con_inputs,Rdiagfull,pdata->Rdiag);
      pdata->Rdiag += VectorXd::Constant(pdata->con_inputs.size(),1,REG);
      pdata->Rdiaginv = pdata->Rdiag.cwiseInverse();
    }    

     // get the map ptr back from matlab
     if (!mxIsNumeric(prhs[6]) || mxGetNumberOfElements(prhs[6])!=1)
     mexErrMsgIdAndTxt("DRC:QPControllermex:BadInputs","the seventh argument should be the map ptr");
     memcpy(&pdata->map_ptr,mxGetPr(prhs[6]),sizeof(pdata->map_ptr));
    
//    pdata->map_ptr = NULL;
    if (!pdata->map_ptr)
      mexWarnMsgTxt("Map ptr is NULL.  Assuming flat terrain at z=0");
    
    // get the multi-robot ptr back from matlab
    if (!mxIsNumeric(prhs[7]) || mxGetNumberOfElements(prhs[7])!=1)
    mexErrMsgIdAndTxt("DRC:QPControllermex:BadInputs","the eigth argument should be the map ptr");
    memcpy(&pdata->multi_robot,mxGetPr(prhs[7]),sizeof(pdata->multi_robot));

    // create gurobi environment
    error = GRBloadenv(&(pdata->env),NULL);

    // set solver params (http://www.gurobi.com/documentation/5.5/reference-manual/node798#sec:Parameters)
    mxArray* psolveropts = myGetProperty(pobj,"solver_options");
    int method = (int) mxGetScalar(myGetField(psolveropts,"method"));
    CGE ( GRBsetintparam(pdata->env,"outputflag",0), pdata->env );
    CGE ( GRBsetintparam(pdata->env,"method",2), pdata->env );
    CGE ( GRBsetintparam(pdata->env,"presolve",0), pdata->env );
    if (method==2) {
    	CGE ( GRBsetintparam(pdata->env,"bariterlimit",20), pdata->env );
    	CGE ( GRBsetintparam(pdata->env,"barhomogeneous",0), pdata->env );
    	CGE ( GRBsetdblparam(pdata->env,"barconvtol",0.0005), pdata->env );
    }

    mxClassID cid;
    if (sizeof(pdata)==4) cid = mxUINT32_CLASS;
    else if (sizeof(pdata)==8) cid = mxUINT64_CLASS;
    else mexErrMsgIdAndTxt("Drake:constructModelmex:PointerSize","Are you on a 32-bit machine or 64-bit machine??");
    
    plhs[0] = mxCreateNumericMatrix(1,1,cid,mxREAL);
    memcpy(mxGetData(plhs[0]),&pdata,sizeof(pdata));
    
    int nq_free = pdata->free_dof.size();

    // preallocate some memory
    pdata->H.resize(nq,nq);
    pdata->C.resize(nq);
  	pdata->J.resize(3,nq);
    pdata->Jdot.resize(3,nq);
    pdata->H_con.resize(nq_con,nq);
    pdata->H_free.resize(nq_free,nq);
    pdata->C_con.resize(nq_con);
    pdata->C_free.resize(nq_free);
    pdata->qdd_free.resize(nq_free);
    pdata->q_ddot_des_free.resize(nq_free);
    pdata->qd_con.resize(nq_con);
    pdata->Hqp_free.resize(nq_free,nq_free);
    pdata->fqp_free.resize(nq_free);
    pdata->J_free.resize(2,nq_free);
    pdata->Jdot_free.resize(2,nq_free);
    pdata->qd_free.resize(nq_free);
    pdata->q_ddot_des_con.resize(nq_con);
    pdata->Hqp_con.resize(nq_con,nq_con);
    pdata->fqp_con.resize(nq_con);
    pdata->J_con.resize(2,nq_con);
    pdata->Jdot_con.resize(2,nq_con);
    return;
  }
  
  // first get the ptr back from matlab
  if (!mxIsNumeric(prhs[0]) || mxGetNumberOfElements(prhs[0])!=1)
    mexErrMsgIdAndTxt("DRC:QPControllermex:BadInputs","the first argument should be the ptr");
  memcpy(&pdata,mxGetData(prhs[0]),sizeof(pdata));

//  for (i=0; i<pdata->r->num_bodies; i++)
//  	mexPrintf("body %d (%s) has %d contact points\n", i, pdata->r->bodies[i].linkname.c_str(), pdata->r->bodies[i].contact_pts.cols());

  int nu = pdata->B.cols(), 
      nq = pdata->r->num_dof,
      nq_free = pdata->free_dof.size(),
      nq_con = pdata->con_dof.size(),
      nu_con = pdata->con_inputs.size();
  const int dim = 3, // 3D
      nd = 2*m_surface_tangents; // for friction cone approx, hard coded for now
  
  assert(nq_con+nq_free == nq);
  assert(nu_con+pdata->free_inputs.size() == nu);

  int narg=1;

  Map< VectorXd > q_ddot_des(mxGetPr(prhs[narg++]),nq);
  
  double *q = mxGetPr(prhs[narg++]);
  double *qd = &q[nq];
  
  int desired_support_argid = narg++;
  
  assert(mxGetM(prhs[narg])==4); assert(mxGetN(prhs[narg])==4);
  Map< Matrix4d > A_ls(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==4); assert(mxGetN(prhs[narg])==2);
  Map< Matrix<double,4,2> > B_ls(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==2); assert(mxGetN(prhs[narg])==2);
  Map< Matrix2d > Qy(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==2); assert(mxGetN(prhs[narg])==2);
  Map< Matrix2d > R_ls(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==2); assert(mxGetN(prhs[narg])==4);
  Map< Matrix<double,2,4> > C_ls(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==2); assert(mxGetN(prhs[narg])==2);
  Map< Matrix2d > D_ls(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==4); assert(mxGetN(prhs[narg])==4);
  Map< Matrix4d > S(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==4); assert(mxGetN(prhs[narg])==1);
  Map< Vector4d > s1(mxGetPr(prhs[narg++]));
  
  assert(mxGetM(prhs[narg])==4); assert(mxGetN(prhs[narg])==1);
  Map< Vector4d > x0(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==2); assert(mxGetN(prhs[narg])==1);
  Map< Vector2d > u0(mxGetPr(prhs[narg++]));

  assert(mxGetM(prhs[narg])==2); assert(mxGetN(prhs[narg])==1);
  Map< Vector2d > y0(mxGetPr(prhs[narg++]));

  double mu = mxGetScalar(prhs[narg++]);

  double* double_contact_sensor = mxGetPr(prhs[narg]); int len = mxGetNumberOfElements(prhs[narg++]);
  VectorXi contact_sensor(len);  
  for (i=0; i<len; i++)
    contact_sensor(i)=(int)double_contact_sensor[i];
  double contact_threshold = mxGetScalar(prhs[narg++]);
  double terrain_height = mxGetScalar(prhs[narg++]); // nonzero if we're using DRCFlatTerrainMap

  Matrix2d R_DQyD_ls = R_ls + D_ls.transpose()*Qy*D_ls;
  
  pdata->r->doKinematics(q,false,qd);
  
  //---------------------------------------------------------------------
  // Compute active support from desired supports -----------------------

  vector<SupportStateElement> active_supports;
  int num_active_contact_pts=0;
  if (!mxIsEmpty(prhs[desired_support_argid])) {
    VectorXd phi;
    mxArray* mxBodies = mxGetProperty(prhs[desired_support_argid],0,"bodies");
    if (!mxBodies) mexErrMsgTxt("couldn't get bodies");
    double* pBodies = mxGetPr(mxBodies);
    mxArray* mxContactPts = mxGetProperty(prhs[desired_support_argid],0,"contact_pts");
    if (!mxContactPts) mexErrMsgTxt("couldn't get contact points");
    for (i=0; i<mxGetNumberOfElements(mxBodies);i++) {
      mxArray* mxBodyContactPts = mxGetCell(mxContactPts,i);
      int nc = mxGetNumberOfElements(mxBodyContactPts);
      if (nc<1) continue;
      
      SupportStateElement se;
      se.body_idx = (int) pBodies[i]-1;
      pr = mxGetPr(mxBodyContactPts); 
      for (j=0; j<nc; j++) {
//      	mexPrintf("adding pt %d to body %d\n", (int)pr[j]-1, se.body_idx);
        se.contact_pt_inds.insert((int)pr[j]-1);
      }
      
      if (contact_threshold == -1) { // ignore terrain
        if (contact_sensor(i)!=0) { // no sensor info, or sensor says yes contact
          active_supports.push_back(se);
          num_active_contact_pts += nc;
      }
      } else {
        contactPhi(pdata,se,phi,terrain_height);
        if (phi.minCoeff()<=contact_threshold || contact_sensor(i)==1) { // any contact below threshold (kinematically) OR contact sensor says yes contact
          active_supports.push_back(se);
          num_active_contact_pts += nc;
        }
      }
    }
  }

  pdata->r->HandC(q,qd,(MatrixXd*)NULL,pdata->H,pdata->C,(MatrixXd*)NULL,(MatrixXd*)NULL);
  getRows(pdata->con_dof,pdata->H,pdata->H_con);
  getRows(pdata->con_dof,pdata->C,pdata->C_con);
  if (nq_free>0) {
    getRows(pdata->free_dof,pdata->H,pdata->H_free);
    getRows(pdata->free_dof,pdata->C,pdata->C_free);
  }
  
  Vector3d xcom;
  // consider making all J's into row-major
  
  pdata->r->getCOM(xcom);
  pdata->r->getCOMJac(pdata->J);
  pdata->r->getCOMJacDot(pdata->Jdot);

  Map<VectorXd> qdvec(qd,nq);
  getRows(pdata->con_dof,qdvec,pdata->qd_con);
  
  MatrixXd Jz,Jp,Jpdot,D;
  int nc = contactConstraints(pdata,num_active_contact_pts,active_supports,Jz,D,Jp,Jpdot,terrain_height);
  int neps = nc*dim;

  Vector4d x_bar,xlimp;
  MatrixXd Jz_con(Jz.rows(),nq_con),Jp_con(Jp.rows(),nq_con),Jpdot_con(Jpdot.rows(),nq_con),D_con(nq_con,D.cols());
  if (nc>0) {
    xlimp << xcom.topRows(2),pdata->J.topRows(2)*qdvec;
    x_bar << xlimp.topRows(2)-x0.topRows(2),xlimp.bottomRows(2)-x0.bottomRows(2);
    getCols(pdata->con_dof,Jz,Jz_con);
    getRows(pdata->con_dof,D,D_con);
    getCols(pdata->con_dof,Jp,Jp_con);
    getCols(pdata->con_dof,Jpdot,Jpdot_con);
  }
  
  //---------------------------------------------------------------------
  // Free DOF cost function ----------------------------------------------
  
  if (nq_free > 0) {

    if (nc > 0) {
      getRows(pdata->free_dof,q_ddot_des,pdata->q_ddot_des_free);
      
      getCols(pdata->free_dof,pdata->J.topRows(2),pdata->J_free);
      getCols(pdata->free_dof,pdata->Jdot.topRows(2),pdata->Jdot_free);
      getRows(pdata->free_dof,qdvec,pdata->qd_free);
      
      // approximate quadratic cost for free dofs with the appropriate matrix block
      pdata->Hqp_free = pdata->J_free.transpose()*R_DQyD_ls*pdata->J_free;
      pdata->Hqp_free += pdata->w*MatrixXd::Identity(nq_free,nq_free);
              
      pdata->fqp_free = (C_ls*xlimp).transpose()*Qy*D_ls*pdata->J_free;
      pdata->fqp_free += (pdata->Jdot_free*pdata->qd_free).transpose()*R_DQyD_ls*pdata->J_free;
      pdata->fqp_free += (S*x_bar + 0.5*s1).transpose()*B_ls*pdata->J_free;
      pdata->fqp_free -= u0.transpose()*R_DQyD_ls*pdata->J_free;
      pdata->fqp_free -= y0.transpose()*Qy*D_ls*pdata->J_free;
      pdata->fqp_free -= pdata->w*pdata->q_ddot_des_free.transpose();
      
      // solve for qdd_free unconstrained
      pdata->qdd_free = -pdata->Hqp_free.ldlt().solve(pdata->fqp_free.transpose());
    } else {
      // qdd_free = q_ddot_des_free;
      getRows(pdata->free_dof,q_ddot_des,pdata->qdd_free);
    }        
    
  }

  int nf = nc+nc*nd; // number of contact force variables
  int nparams = nq_con+nu_con+nf+neps;
  
  // set obj,lb,up
  VectorXd lb(nparams), ub(nparams);
  lb.head(nq_con) = -1e3*VectorXd::Ones(nq_con);
  ub.head(nq_con) = 1e3*VectorXd::Ones(nq_con);
  lb.segment(nq_con,nu_con) = pdata->umin_con;
  ub.segment(nq_con,nu_con) = pdata->umax_con;
  lb.segment(nq_con+nu_con,nf) = VectorXd::Zero(nf);
  ub.segment(nq_con+nu_con,nf) = 500*VectorXd::Ones(nf);
  lb.tail(neps) = -pdata->slack_limit*VectorXd::Ones(neps);
  ub.tail(neps) = pdata->slack_limit*VectorXd::Ones(neps);
  
  //----------------------------------------------------------------------
  // QP cost function ----------------------------------------------------
  //
  //  min: quad(Jdot*qd + J*qdd,R_ls)+quad(C*x+D*(Jdot*qd + J*qdd),Qy) + (2*x'*S + s1')*(A*x + B*(Jdot*qd + J*qdd)) + w*quad(qddot_ref - qdd) + quad(u,R) + quad(epsilon)
  MatrixXd Qnfdiag = MatrixXd::Constant(nf,1,REG);  MatrixXd Qneps = MatrixXd::Constant(neps,1,.001+REG);
//  MatrixXd Qinvnq(nq_con,nq_con);  VectorXd Qinvnfdiag = ??; Qinvneps = VectorXd::Constant(neps,1/.001);
  VectorXd f(nparams);
  {      
    getRows(pdata->con_dof,q_ddot_des,pdata->q_ddot_des_con);
    if (nc > 0) {
      getCols(pdata->con_dof,pdata->J.topRows(2),pdata->J_con);
      getCols(pdata->con_dof,pdata->Jdot.topRows(2),pdata->Jdot_con);

      // Q(1:nq_con,1:nq_con) = Hqp_con in the matlab code, Qnq here
      pdata->Hqp_con = pdata->J_con.transpose()*R_DQyD_ls*pdata->J_con;          // note: only needed for gurobi call (could pull it down)
      pdata->Hqp_con += (pdata->w+REG)*MatrixXd::Identity(nq_con,nq_con);
//    but we actually want Qinvnq, which I can compute efficiently using the
//    matrix inversion lemma (see wikipedia):
//    inv(A + U'CV) = inv(A) - inv(A)*U* inv([ inv(C)+ V*inv(A)*U ]) V inv(A)
//     but inv(A) is 1/w*eye(nq_con), so I can reduce this to:
//        = 1/w ( eye(nq_con) - 1/w*U* inv[ inv(C) + 1/w*V*U ] * V
//      double wi = 1/pdata->w;
//      Qinvnq = wi*MatrixXd::Identity(nq_con,nq_con) - wi*wi*J_con.transpose()*(R_DQyD_ls.inverse() + wi*J_con*Jcon.trasnpose()).inverse()*J_con;

      pdata->fqp_con = (C_ls*xlimp).transpose()*Qy*D_ls*pdata->J_con;
      pdata->fqp_con += (pdata->Jdot_con*pdata->qd_con).transpose()*R_DQyD_ls*pdata->J_con;
      pdata->fqp_con += (S*x_bar + 0.5*s1).transpose()*B_ls*pdata->J_con;
      pdata->fqp_con -= u0.transpose()*R_DQyD_ls*pdata->J_con;
      pdata->fqp_con -= y0.transpose()*Qy*D_ls*pdata->J_con;
      pdata->fqp_con -= pdata->w*pdata->q_ddot_des_con.transpose();

      // obj(1:nq_con) = fqp_con
      f.topRows(nq_con) = pdata->fqp_con.transpose();
     } else {
      // Q(1:nq_con,1:nq_con) = eye(nq_con)
    	pdata->Hqp_con = MatrixXd::Constant(nq_con,1,1+REG);

      // obj(1:nq_con) = -qddot_des_con
      f.topRows(nq_con) = -pdata->q_ddot_des_con;
    } 
  }
  f.bottomRows(nu_con+nf+neps) = VectorXd::Zero(nu_con+nf+neps);

  vector< MatrixXd* > QBlkDiag( nc>0 ? 4 : 2 );  // nq, nu, nf, neps   // this one is for gurobi
//  vector< Map<MatrixXd> > QinvBlkDiag;  // nq, nu, nf, neps  // this one is for fastQP
  QBlkDiag[0] = &pdata->Hqp_con;
//  QinvBlkDiag[0] = Map<MatrixXd>(Qinvnq.data(),nq_con,nq_con);
  QBlkDiag[1] = &pdata->Rdiag;      // quadratic input cost Q(nq_con+(1:nu_con),nq_con+(1:nu_con))=R
//  QinvBlkDiag[1] = Map<MatrixXd>(pdata->Rdiaginv.data(),nu_con,1);      // quadratic input cost Q(nq_con+(1:nu_con),nq_con+(1:nu_con))=R
  if (nc>0) {
  	QBlkDiag[2] = &Qnfdiag;
//  	QinvBlkDiag[2] = Map<MatrixXd>(Qinvnfdiag.data(),nf,1);
  	QBlkDiag[3] = &Qneps;     // quadratic slack var cost, Q(nparams-neps:end,nparams-neps:end)=eye(neps)
//  	QinvBlkDiag[3] = Map<MatrixXd>(Qinvneps.data(),neps,1);     // quadratic slack var cost, Q(nparams-neps:end,nparams-neps:end)=eye(neps)
  }

  MatrixXd Aeq(nq_con+neps,nparams);
  Aeq.topRightCorner(nq_con+neps,neps) = MatrixXd::Zero(nq_con+neps,neps);  // note: obvious sparsity here
  VectorXd beq(nq_con+neps);
  
  { // constrained dynamics
    //  H*qdd - B*u - J*lambda - Dbar*beta = -C
    MatrixXd H_con_con(nq_con,nq_con), H_con_free(nq_con,nq_free);
    getCols(pdata->con_dof,pdata->H_con,H_con_con);
    Aeq.topLeftCorner(nq_con,nq_con) = H_con_con;  // Aeq(1:nq_con,1:nq_con) = H_con_con
    Aeq.block(0,nq_con,nq_con,nu_con) = -pdata->B_con;
    beq.topRows(nq_con) = -pdata->C_con;
    
    if (nc>0) {
      Aeq.block(0,nq_con+nu_con,nq_con,nc) = -Jz_con.transpose();
      Aeq.block(0,nq_con+nu_con+nc,nq_con,nc*nd) = -D_con;
    }
    if (nq_free>0) {
      getCols(pdata->free_dof,pdata->H_con,H_con_free);
      beq.topRows(nq_con) -= H_con_free*pdata->qdd_free;
    }      
  }

  if (nc > 0) {
    // relative acceleration constraint
    Aeq.bottomLeftCorner(neps,nq_con) = Jp_con;
    Aeq.block(nq_con,nq_con,neps,nu_con+nf) = MatrixXd::Zero(neps,nu_con+nf);  // note: obvious sparsity here
    Aeq.bottomRightCorner(neps,neps) = MatrixXd::Identity(neps,neps);             // note: obvious sparsity here
    beq.bottomRows(neps) = (-Jpdot_con - 1.0*Jp_con)*pdata->qd_con;
  }    

  MatrixXd Ain = MatrixXd::Zero(nc,nparams);  // note: obvious sparsity here
  VectorXd bin = VectorXd::Zero(nc);
  if (nc>0) {
    // linear friction constraints
    int cind[1+nd];
    for (i=0; i<nc; i++) {
      // -mu*lambda[i] + sum(beta[i]s) <= 0
      Ain(i,nq_con+nu_con+i) = -mu;
      for (j=0; j<nd; j++) Ain(i,nq_con+nu_con+nc+i*nd+j) = 1;
    }
  }    
  
  VectorXd alpha(nparams);
  set<int> active;

  GRBmodel * model = gurobiQP(pdata->env,QBlkDiag,f,Aeq,beq,Ain,bin,lb,ub,active,alpha);

  //----------------------------------------------------------------------
  // Solve for free inputs -----------------------------------------------
  VectorXd y(nu);
  VectorXd qdd(nq);
  if (nq_free > 0) {
    set<int>::iterator iter;
    i=0;
    for (iter=pdata->free_dof.begin(); iter!=pdata->free_dof.end(); iter++)
      qdd(*iter) = pdata->qdd_free(i++);
    i=0;
    for (iter=pdata->con_dof.begin(); iter!=pdata->con_dof.end(); iter++)
      qdd(*iter) = alpha(i++);
  
    VectorXd u_free = pdata->B_free.jacobiSvd(ComputeThinU|ComputeThinV).solve(pdata->H_free*qdd + pdata->C_free);

    i=0;
    for (iter=pdata->free_inputs.begin(); iter!=pdata->free_inputs.end(); iter++)
      y(*iter) = u_free(i++);
    i=0;
    for (iter=pdata->con_inputs.begin(); iter!=pdata->con_inputs.end(); iter++) {
      y(*iter) = alpha(nq_con + i);
      i++;
    }    
    
    // saturate inputs
    ArrayXd tmp = pdata->umin.max(y.array());
    y = tmp.min(pdata->umax).matrix();
  } else {
    y = alpha.segment(nq,nu);
    qdd = alpha.segment(0,nq);
  }

  
  if (nlhs>0) plhs[0] = eigenToMatlab(y);

  if (nlhs>1) {
    double Vdot;
    if (nc>0) 
      Vdot = (2*x_bar.transpose()*S + s1.transpose())*(A_ls*x_bar + B_ls*(pdata->Jdot.topRows(2)*qdvec + pdata->J.topRows(2)*qdd));
    else
      Vdot = 0;
    plhs[1] = mxCreateDoubleScalar(Vdot);
  }

  if (nlhs>2) {
      plhs[2] = mxCreateDoubleMatrix(1,active_supports.size(),mxREAL);
      pr = mxGetPr(plhs[2]);
      int i=0;
      for (vector<SupportStateElement>::iterator iter = active_supports.begin(); iter!=active_supports.end(); iter++) {
          pr[i++] = (double) (iter->body_idx + 1);
      }
  }

  if (nlhs>3) {  // return model.Q (for unit testing)
    int qnz;
    CGE (GRBgetintattr(model,"NumQNZs",&qnz), pdata->env);
    int *qrow = new int[qnz], *qcol = new int[qnz];
    double* qval = new double[qnz];
    CGE (GRBgetq(model,&qnz,qrow,qcol,qval), pdata->env);
    plhs[3] = mxCreateDoubleMatrix(nparams,nparams,mxREAL);
    double* pm = mxGetPr(plhs[3]);
    memset(pm,0,sizeof(double)*nparams*nparams);
    for (i=0; i<qnz; i++)
      pm[qrow[i]+nparams*qcol[i]] = qval[i];
    delete[] qrow;
    delete[] qcol;
    delete[] qval;

    if (nlhs>4) {  // return model.obj (for unit testing)
      plhs[4] = mxCreateDoubleMatrix(1,nparams,mxREAL);
      CGE (GRBgetdblattrarray(model, "Obj", 0, nparams, mxGetPr(plhs[4])), pdata->env);

      if (nlhs>5) {  // return model.A (for unit testing)
        int numcon;
        CGE (GRBgetintattr(model,"NumConstrs",&numcon), pdata->env);
        plhs[5] = mxCreateDoubleMatrix(numcon,nparams,mxREAL);
        double *pm = mxGetPr(plhs[5]);
        for (i=0; i<numcon; i++)
          for (j=0; j<nparams; j++)
            CGE (GRBgetcoeff(model,i,j,&pm[i+j*numcon]), pdata->env);

        if (nlhs>6) {  // return model.rhs (for unit testing)
          plhs[6] = mxCreateDoubleMatrix(numcon,1,mxREAL);
          CGE (GRBgetdblattrarray(model,"RHS",0,numcon,mxGetPr(plhs[6])), pdata->env);
        } 
        
        if (nlhs>7) { // return model.sense
          char* sense = new char[numcon+1];
          CGE (GRBgetcharattrarray(model,"Sense",0,numcon,sense), pdata->env);
          sense[numcon]='\0';
          plhs[7] = mxCreateString(sense);
          // delete[] sense;  // it seems that I'm not supposed to free this
        }
        
        if (nlhs>8) {
          plhs[8] = mxCreateDoubleMatrix(nparams,1,mxREAL);
          CGE (GRBgetdblattrarray(model, "LB", 0, nparams, mxGetPr(plhs[8])), pdata->env);
        }
        if (nlhs>9) {
          plhs[9] = mxCreateDoubleMatrix(nparams,1,mxREAL);
          CGE (GRBgetdblattrarray(model, "UB", 0, nparams, mxGetPr(plhs[9])), pdata->env);
        }
      }
    }
  }
  
  GRBfreemodel(model);

  //  GRBfreeenv(env);
} 
