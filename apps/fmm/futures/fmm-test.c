#include "fmm.h"

int main(int argc, char **argv)
{
  int nsources = 10000;
  int ntargets = 10000;
  int accuracy = 3;
  int datatype = 1;
  int s = 40;

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
	switch (argv[i][1]) {
	case 'n': nsources = atoi(argv[++i]); break;
	case 'm': ntargets = atoi(argv[++i]); break;
	case 'a': accuracy = atoi(argv[++i]); break;
	case 'd': datatype = atoi(argv[++i]); break;
	case 's': s = atoi(argv[++i]); break;
	default: break;
	}
      }
    }
  }

  if (accuracy == 3 || accuracy == 6) {
  } else {
    printf("Only '-a 3' and '-a 6' supported\n");
    return -1;
  }

  double *sources = calloc(nsources*3, sizeof(double)); 
  double *charges = calloc(nsources, sizeof(double)); 
  double *targets = calloc(ntargets*3, sizeof(double));
  double *potential = calloc(ntargets, sizeof(double));
  double *field = calloc(ntargets*3, sizeof(double)); 

  if (sources == 0 || charges == 0 || targets == 0 || potential == 0 || field == 0) {
    fprintf(stderr, "Memory allocation failure\n"); 
    return -1;
  }

  double pi = acos(-1); 
  if (datatype == 1) {
    if (nsources != ntargets) {
      fprintf(stderr, "nsources == ntargets required for type-1 data\n");
      return -1;
    }
    
    for (int i = 0; i < nsources; i++) {
      int j = 3*i;
      charges[i] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j] = targets[j] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j + 1] = targets[j + 1] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j + 2] = targets[j + 2] = 1.0*rand()/RAND_MAX - 0.5;	
    }
  } else if (datatype == 2) {
    if (nsources != ntargets) {
      fprintf(stderr, "nsources == ntargets required for type-2 data\n");
      return -1;
    }

    for (int i = 0; i < nsources; i++) {
      int j = 3*i;
      double theta = 1.0*rand()/RAND_MAX*pi;
      double phi = 1.0*rand()/RAND_MAX*pi*2;
      
      charges[i] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j] = sin(theta)*cos(phi);
      sources[j + 1] = sin(theta)*sin(phi);
      sources[j + 2] = cos(theta);

      targets[j] = sources[j];
      targets[j + 1] = sources[j + 1];
      targets[j + 2] = sources[j + 2];
    }
  } else if (datatype == 3) {
    for (int i = 0; i < nsources; i++) {
      int j = 3*i;
      double t = 1.0*rand()/RAND_MAX*pi*2;
      double u = 1.0*rand()/RAND_MAX*pi*2;
      
      charges[i] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j] = cos(t)*(2 + 0.5*cos(u));
      sources[j + 1] = sin(t)*(2 + 0.5*cos(u));
      sources[j + 2] = 0.5*sin(u);
    }
    
    for (int i = 0; i < ntargets; i++) {
      int j = 3*i;
      double t = 1.0*rand()/RAND_MAX*pi*2;
      double u = 1.0*rand()/RAND_MAX*pi*2;
      
      targets[j] = cos(t)*(2 + 0.5*cos(u));
      targets[j + 1] = sin(t)*(2 + 0.5*cos(u));
      targets[j + 2] = 0.5*sin(u);
    }
  }
  
  
  nsources_ = nsources; 
  ntargets_ = ntargets; 

  sources_   = calloc(3*nsources_, sizeof(double));
  charges_   = calloc(nsources_, sizeof(double));
  targets_   = calloc(3*ntargets_, sizeof(double));
  potential_ = calloc(ntargets_, sizeof(double));
  field_     = calloc(3*ntargets_, sizeof(double));

  if (accuracy == 3) {
    pterms_ = nlambs_ = 9; 
    pgsz_ = 100; 
  } else if (accuracy == 6) {
    pterms_ = nlambs_ = 18; 
    pgsz_ = 361;
  }

  numphys_   = calloc(nlambs_, sizeof(int));
  numfour_   = calloc(nlambs_, sizeof(int));
  whts_      = calloc(nlambs_, sizeof(double));
  rlams_     = calloc(nlambs_, sizeof(double));
  rdplus_    = calloc(pgsz_*(2*pterms_ + 1), sizeof(double));
  rdminus_   = calloc(pgsz_*(2*pterms_ + 1), sizeof(double));
  rdsq3_     = calloc(pgsz_*(2*pterms_ + 1), sizeof(double));
  rdmsq3_    = calloc(pgsz_*(2*pterms_ + 1), sizeof(double));
  dc_        = calloc((2*pterms_ + 1)*(2*pterms_ + 1)*(2*pterms_ + 1), sizeof(double));
  ytopc_     = calloc(pgsz_, sizeof(double));
  ytopcs_    = calloc(pgsz_, sizeof(double));
  ytopcsinv_ = calloc(pgsz_, sizeof(double));
  rlsc_      = calloc(pgsz_*nlambs_, sizeof(double));  

  frmini(); 
  rotgen(); 
  vwts(); 
  numthetahalf();
  numthetafour(); 
  rlscini(); 

  nexptot_  = 0; 
  nthmax_   = 0;
  nexptotp_ = 0; 
  for (int i = 1; i <= nlambs_; i++) {
    nexptot_ += numfour_[i - 1];
    if (numfour_[i - 1] > nthmax_)
      nthmax_ = numfour_[i - 1];
    nexptotp_ += numphys_[i - 1];
  }
  nexptotp_ /= 2.0;
  nexpmax_ = fmax(nexptot_, nexptotp_) + 1;

  xs_ = calloc(nexpmax_*3, sizeof(double complex));
  ys_ = calloc(nexpmax_*3, sizeof(double complex));
  zs_ = calloc(nexpmax_*3, sizeof(double));

  fexpe_    = calloc(15000, sizeof(double complex));
  fexpo_    = calloc(15000, sizeof(double complex));
  fexpback_ = calloc(15000, sizeof(double complex));

  mkfexp(); 
  mkexps();  

  BuildGraph(sources, nsources, targets, ntargets, s); 
  printf("\n\n%-40s%40d\n" "%-40s%40d\n" "%-40s%40d\n" "%-40s%40d\n", 
	 "Level of source tree:", nslev_, 
	 "Total number of source boxes:", nsboxes_, 
	 "Level of target tree:", ntlev_, 
	 "Total number of target boxes:", ntboxes_);

  mpole_ = calloc((1 + nsboxes_)*pgsz_, sizeof(double complex)); 
  local_ = calloc((1 + ntboxes_)*pgsz_, sizeof(double complex)); 
  expu_  = calloc((1 + nsboxes_)*nexpmax_, sizeof(double complex)); 
  expd_  = calloc((1 + nsboxes_)*nexpmax_, sizeof(double complex)); 
  expn_  = calloc((1 + nsboxes_)*nexpmax_, sizeof(double complex)); 
  exps_  = calloc((1 + nsboxes_)*nexpmax_, sizeof(double complex)); 
  expe_  = calloc((1 + nsboxes_)*nexpmax_, sizeof(double complex)); 
  expw_  = calloc((1 + nsboxes_)*nexpmax_, sizeof(double complex)); 
  scale_ = calloc(1 + nslev_, sizeof(double)); 

  scale_[0] = 1/size_; 
  for (int i = 1; i <= nslev_; i++) 
    scale_[i] = scale_[i - 1]*2; 

  for (int i = 0; i < nsources; i++) {
    int j = mapsrc_[i], i3 = i*3, j3 = j*3; 
    charges_[i]      = charges[j];
    sources_[i3]     = sources[j3];
    sources_[i3 + 1] = sources[j3 + 1];
    sources_[i3 + 2] = sources[j3 + 2];
  }

  for (int i = 0; i < ntargets; i++) {
    int j = maptar_[i], i3 = i*3, j3 = j*3; 
    targets_[i3]     = targets[j3];
    targets_[i3 + 1] = targets[j3 + 1];
    targets_[i3 + 2] = targets[j3 + 2];
  }

  FMMCompute(); 

  for (int i = 0; i < ntargets; i++) {
    int j = maptar_[i], i3 = i*3, j3 = j*3; 
    potential[j]  = potential_[i]; 
    field[j3]     = field_[i3]; 
    field[j3 + 1] = field_[i3 + 1];
    field[j3 + 2] = field_[i3 + 2];
  }

  int nverify = (ntargets < 200 ? ntargets : 200);
  double salg = 0, salg2 = 0, stot = 0, stot2 = 0, errmax = 0; 

  for (int i = 0; i < nverify; i++) {
    int i3 = i*3; 
    const double *t = &targets[i3]; 
    double pot = 0, fx = 0, fy = 0, fz = 0; 
    for (int j = 0; j < nsources; j++) {
      int j3 = j*3; 
      const double *s = &sources[j3]; 
      const double q = charges[j]; 
      {
	double rx = t[0] - s[0]; 
	double ry = t[1] - s[1];
	double rz = t[2] - s[2]; 
	double rr = rx*rx + ry*ry + rz*rz; 
	double rdis = sqrt(rr); 

	if (rr) {
	  pot += q/rdis; 
	  double rmul = q/(rdis*rr);
	  fx += rmul*rx; 
	  fy += rmul*ry;
	  fz += rmul*rz;
	}
      }
    }

    salg += (potential[i] - pot)*(potential[i] - pot); 
    stot += pot*pot; 
    salg2 += pow(fx - field[i3], 2) + pow(fy - field[i3 + 1], 2) + 
      pow(fz - field[i3 + 2], 2); 
    stot2 += fx*fx + fy*fy + fz*fz; 

    errmax = fmax(errmax, fabs(potential[i] - pot)); 
  }

  printf("%-50s%30.5e\n" "%-50s%30.5e\n" "%-50s%30.5e\n",
	 "Error of potential in L2 norm:", sqrt(salg/stot),
	 "Error of potential in L-infinity norm:", errmax,
	 "Error of field in L2 norm:", sqrt(salg2/stot2));

  DestroyGraph();

  free(mpole_);
  free(local_);
  free(expu_);
  free(expd_);
  free(expn_);
  free(exps_);
  free(expe_);
  free(expw_);
  free(scale_);
  free(sources_);
  free(charges_);
  free(targets_);
  free(potential_);
  free(field_);

  free(numphys_);
  free(numfour_);
  free(whts_);
  free(rlams_);
  free(rdplus_);
  free(rdminus_);
  free(rdsq3_);
  free(rdmsq3_);
  free(dc_);
  free(ytopc_);
  free(ytopcs_);
  free(ytopcsinv_);
  free(rlsc_);
  free(xs_);
  free(ys_);
  free(zs_);
  free(fexpe_);
  free(fexpo_);
  free(fexpback_);


  free(sources);
  free(charges);
  free(targets);
  free(potential);
  free(field);

  return 0;
}
