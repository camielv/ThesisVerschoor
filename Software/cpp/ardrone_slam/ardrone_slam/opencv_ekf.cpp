#include "opencv_ekf.h"
#include <cv.hpp>


namespace cv {

ExtendedKalmanFilter::ExtendedKalmanFilter() {}

ExtendedKalmanFilter::ExtendedKalmanFilter(int dynamParams, int measureParams, int controlParams, int type)
{
    init(dynamParams, measureParams, controlParams, type);

	/* Nick */
	yaw_offset				= 0.0f;
	running					= false;
	last_update				= 0.0;
	hMutex = CreateMutex(NULL, FALSE, NULL);
}

void ExtendedKalmanFilter::init(int DP, int MP, int CP, int type)
{
    CV_Assert( DP > 0 && MP > 0 );
    CV_Assert( type == CV_32F || type == CV_64F );
    CP = std::max(CP, 0);

    statePre = Mat::zeros(DP, 1, type);
    statePost = Mat::zeros(DP, 1, type);
    transitionMatrix = Mat::eye(DP, DP, type);

    processNoiseCov = Mat::eye(DP, DP, type);
    measurementMatrix = Mat::zeros(MP, DP, type);
    measurementNoiseCov = Mat::eye(MP, MP, type);

    errorCovPre = Mat::zeros(DP, DP, type);
    errorCovPost = Mat::zeros(DP, DP, type);
    gain = Mat::zeros(DP, MP, type);

    if( CP > 0 )
        controlMatrix = Mat::zeros(DP, CP, type);
    else
        controlMatrix.release();

    temp1.create(DP, DP, type);
    temp2.create(MP, DP, type);
    temp3.create(MP, MP, type);
    temp4.create(MP, DP, type);
    temp5.create(MP, 1, type);
}

const Mat& ExtendedKalmanFilter::predict(const Mat& control)
{
    // update the state: x'(k) = A*x(k)
    statePre = transitionMatrix*statePost;

    if( control.data )
        // x'(k) = x'(k) + B*u(k)
        statePre += controlMatrix*control;

    // update error covariance matrices: temp1 = A*P(k)
    temp1 = transitionMatrix*errorCovPost;

    // P'(k) = temp1*At + Q
    gemm(temp1, transitionMatrix, 1, processNoiseCov, 1, errorCovPre, GEMM_2_T);

    return statePre;
}

const Mat& ExtendedKalmanFilter::correct(const Mat& measurement /*NICK */)
{
    // temp2 = H*P'(k)
    temp2 = measurementMatrix * errorCovPre;

    // temp3 = temp2*Ht + R
    gemm(temp2, measurementMatrix, 1, measurementNoiseCov, 1, temp3, GEMM_2_T); 

    // temp4 = inv(temp3)*temp2 = Kt(k)
    solve(temp3, temp2, temp4, DECOMP_SVD);

    // K(k)
    gain = temp4.t();
    
    // temp5 = z(k) - H*x'(k)
    temp5 = measurement - measurementMatrix*statePre;

    // x(k) = x'(k) + K(k)*temp5
    statePost = statePre + gain*temp5;

    // P(k) = P'(k) - K(k)*temp2
    errorCovPost = errorCovPre - gain*temp2;

    return statePost;
}


void ExtendedKalmanFilter::lock()
{
	WaitForSingleObject(hMutex, 2000);
}


void ExtendedKalmanFilter::release()
{
	ReleaseMutex(hMutex);
}


void ExtendedKalmanFilter::start(double time, float yaw)
{
	last_update		= time;
	yaw_offset		= yaw;
	running			= true;
}


double ExtendedKalmanFilter::difftime(double time)
{
	double tmp = last_update;
	last_update = time;
	return max(0.000001, time - tmp);
}

}