#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // state vector dimension
  n_x_ = 5;

  // initial state vector
  x_ = VectorXd(n_x_);

  // initial covariance matrix
  P_ = MatrixXd(n_x_, n_x_);
  P_ << 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
        0, 0, 0, 0, 1;

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2.5; // 1/2 of 5ms2

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.9;

  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
   * End DO NOT MODIFY section for measurement noise values 
   */

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  lambda_ = 3 - n_x_;
  n_aug_ = n_x_ + 2;
  n_sig_ = 2 * n_aug_ + 1;


  weights_ = VectorXd(n_sig_);
  weights_(0) = lambda_ / (lambda_ + n_aug_);
  for(int i=0; i < n_sig_; i++)
  {
    weights_(i) = 0.5 / (lambda_ + n_aug_);	
  }
  
  R_radar_ = MatrixXd(3, 3);
  R_radar_ << std_radr_*std_radr_, 0, 0,
              0, std_radphi_*std_radphi_, 0,
              0, 0,std_radrd_*std_radrd_;

  R_lidar_ = MatrixXd(2, 2);
  R_lidar_ << std_laspx_*std_laspx_,0,
              0,std_laspy_*std_laspy_;

}

UKF::~UKF() {}


void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measuremenweights_ts.
   */
  if ( !is_initialized_) {
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
 
      double x = meas_package.raw_measurements_[0] * cos(meas_package.raw_measurements_[1]);
      double y = meas_package.raw_measurements_[0] * sin(meas_package.raw_measurements_[1]);

          double vx = meas_package.raw_measurements_[2] * cos(meas_package.raw_measurements_[1]);
  	  double vy = meas_package.raw_measurements_[2] * sin(meas_package.raw_measurements_[1]);
      double v = sqrt(vx * vx + vy * vy);
      x_ << x, y, v, 0, 0;
    } else {
      x_ << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], 0, 0, 0;
    }

    time_us_ = meas_package.timestamp_ ;
    is_initialized_ = true;
    return;
  }

  double time_diff = (meas_package.timestamp_ - time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;
  Prediction(time_diff);

  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    UpdateRadar(meas_package);
  }
  if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
    UpdateLidar(meas_package);
  }
}

void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */

    //std::cout << "x_ " << x_ << std::endl;

    //std::cout << "P_ " << P_ << std::endl;

    //std::cout << "P_ " << P_ << std::endl;


	  VectorXd x_aug = VectorXd(n_aug_);
	  x_aug.head(5) = x_;
	  x_aug(5) = 0;
	  x_aug(6) = 0;

	  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
	  P_aug.fill(0.0);
	  P_aug.topLeftCorner(n_x_,n_x_) = P_;
	  P_aug(5,5) = std_a_*std_a_;
	  P_aug(6,6) = std_yawdd_*std_yawdd_;

	  MatrixXd Xsig_aug = MatrixXd( n_aug_, n_sig_ );

	  MatrixXd A = P_aug.llt().matrixL();

	  Xsig_aug.col(0) = x_aug;

	  for (int i = 0; i < n_aug_; i++){
	      Xsig_aug.col( i + 1 ) = x_aug + sqrt(lambda_ + n_aug_) * A.col(i);
	      Xsig_aug.col( i + 1 + n_aug_ ) = x_aug - sqrt(lambda_ + n_aug_) * A.col(i);
	  }

	  MatrixXd Xsig_pred = MatrixXd(n_x_, n_sig_);
	  for (int i = 0; i< n_sig_; i++)
	  {
	    double p_x = Xsig_aug(0,i);
	    double p_y = Xsig_aug(1,i);
	    double v = Xsig_aug(2,i);
	    double yaw = Xsig_aug(3,i);
	    double yawd = Xsig_aug(4,i);
	    double nu_a = Xsig_aug(5,i);
	    double nu_yawdd = Xsig_aug(6,i);

	    double px_p, py_p;

	    if (fabs(yawd) > 0.001) {
		px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
		py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
	    }
	    else {
		px_p = p_x + v*delta_t*cos(yaw);
		py_p = p_y + v*delta_t*sin(yaw);
	    }

	    double v_p = v;
	    double yaw_p = yaw + yawd*delta_t;
	    double yawd_p = yawd;

	    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
	    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
	    v_p = v_p + nu_a*delta_t;

	    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
	    yawd_p = yawd_p + nu_yawdd*delta_t;

	    Xsig_pred(0,i) = px_p;
	    Xsig_pred(1,i) = py_p;
	    Xsig_pred(2,i) = v_p;
	    Xsig_pred(3,i) = yaw_p;
	    Xsig_pred(4,i) = yawd_p;
	  }

	  Xsig_pred_ = Xsig_pred;

	  x_ = Xsig_pred_ * weights_;

	  P_.fill(0.0);
	  for (int i = 0; i < n_sig_; i++) {

	    VectorXd x_diff = Xsig_pred_.col(i) - x_;

	     while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
	     while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

	    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
	  }

}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */

  int n_z = 2;
  MatrixXd Zsig = Xsig_pred_.block(0, 0, n_z, n_sig_);

  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < n_sig_; i++) {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {  

    VectorXd z_diff = Zsig.col(i) - z_pred;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  S = S + R_lidar_;

  VectorXd z = meas_package.raw_measurements_;

  MatrixXd Tc = MatrixXd(n_x_, n_z);

  Tc.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {

    VectorXd z_diff = Zsig.col(i) - z_pred;

    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  MatrixXd K = Tc * S.inverse();

  VectorXd y = z - z_pred;

  x_ = x_ + K * y;
  P_ = P_ - K*S*K.transpose();

}


void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

  int n_z = 3;

  MatrixXd Zsig = MatrixXd(n_z, n_sig_);

  for (int i = 0; i < n_sig_; i++) {  

    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        
    Zsig(1,i) = atan2(p_y,p_x);                                 
    Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   
  }

  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < n_sig_; i++) {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < n_sig_; i++) { 

    VectorXd z_diff = Zsig.col(i) - z_pred;

    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  S = S + R_radar_;

  VectorXd z = meas_package.raw_measurements_;

  MatrixXd Tc = MatrixXd(n_x_, n_z);

  Tc.fill(0.0);
  for (int i = 0; i < n_sig_; i++) {  

    VectorXd z_diff = Zsig.col(i) - z_pred;

    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  MatrixXd K = Tc * S.inverse();

  VectorXd z_diff = z - z_pred;

  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
}

