#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/openni_grabber.h>
#include <pcl/features/integral_image_normal.h>
#include <pcl/segmentation/organized_multi_plane_segmentation.h>
#include <pcl/segmentation/organized_connected_component_segmentation.h>
#include <pcl/features/organized_edge_detection.h>
#include <pcl/segmentation/euclidean_cluster_comparator.h>
#include <pcl/common/time.h>
#include <iostream>
#include <fstream>

#include <omnimapper/time.h>
#include <pcl/segmentation/plane_refinement_comparator.h>

#include <boost/tuple/tuple.hpp>

#include <tbb/task.h>
#include <tbb/pipeline.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/spin_mutex.h>
#include <tbb/tbb_thread.h>

//#include <boost/atomic.hpp>

typedef pcl::PointXYZRGBA PointT;
typedef pcl::PointCloud<PointT> Cloud;
typedef typename Cloud::Ptr CloudPtr;
typedef typename Cloud::ConstPtr CloudConstPtr;
typedef pcl::PointCloud<pcl::Normal> NormalCloud;
typedef typename NormalCloud::Ptr NormalCloudPtr;
typedef typename NormalCloud::ConstPtr NormalCloudConstPtr;
typedef pcl::PointCloud<pcl::Label> LabelCloud;
typedef typename LabelCloud::Ptr LabelCloudPtr;
typedef typename LabelCloud::ConstPtr LabelCloudConstPtr;
typedef boost::posix_time::ptime Time;

// Test of Intel Threading Building Blocks Library for organized feature extraction
// Initial pipeline: Grabber::<Null, CloudPtr> -> NormalEstimation<CloudPtr, <CloudPtr, NormalCloudPtr> > -> OrganizedMultiPlane < <CloudPtr, NormalCloudPtr>, <


// This is the normal estimation task.  Input is a raw point cloud, output is the normals.
template <typename PointT>
class NormalEstimationTask
{
  typedef pcl::PointCloud<PointT> Cloud;
  typedef typename Cloud::Ptr CloudPtr;
  typedef pcl::PointCloud<pcl::Normal> NormalCloud;
  typedef NormalCloud::Ptr NormalCloudPtr;

  protected:
    boost::shared_ptr<pcl::IntegralImageNormalEstimation<PointT, pcl::Normal> > ne_;
    NormalCloudPtr normals_;

  public:
    NormalEstimationTask (boost::shared_ptr<pcl::IntegralImageNormalEstimation<PointT, pcl::Normal> >& ne) 
      : ne_ (ne),
        normals_ (new NormalCloud ())
    {
      ne_.setNormalEstimationMethod (ne_.COVARIANCE_MATRIX);
      ne_.setMaxDepthChangeFactor (0.02f);
      ne_.setNormalSmoothingSize (20.0f);
    }
    
    void
    setCloud (CloudPtr& cloud)
    {
      ne_.setInputCloud (cloud);
    }

    // tbb::task* 
    // execute ()
    // {
    //   ne_.compute (*normals_);
    //   return (NULL);
    // }

    
      
};

// This is the Organized Multi Plane Segmentation task.  Input is a point cloud, output is segments, labels, etc.  As a tuple.
template <typename PointT>
class PlaneSegmentationTask : public tbb::task
{
  protected:
    pcl::OrganizedMultiPlaneSegmentation<PointT, pcl::Normal, pcl::Label> mps_;
  public:
    tbb::task* execute ()
    {
        
      return (NULL);
    }
};

// Filter for ni_grabber, produces input to the pipeline
template <typename PointT>
class GrabberTask
{
  protected:
    boost::shared_ptr<pcl::OpenNIGrabber> ni_grabber_;
    CloudConstPtr cloud_;
    boost::shared_ptr<bool> updated_;
    boost::shared_ptr<tbb::spin_mutex> mutex_;

  public:
    GrabberTask () 
      : ni_grabber_ (new pcl::OpenNIGrabber ("#1")),
        cloud_ (new Cloud ()),
        updated_ (new bool(false)),
        mutex_ (new tbb::spin_mutex ())
    {
      boost::function<void (const CloudConstPtr&)> f = boost::bind (&GrabberTask::cloudCallback, this, _1);
      boost::signals2::connection c = ni_grabber_->registerCallback (f);
      ni_grabber_->start ();
      printf ("Grabber started!\n");
    }

    GrabberTask (const GrabberTask<PointT>& f)
      : ni_grabber_ (f.ni_grabber_),
        cloud_ (f.cloud_),
        updated_ (f.updated_),
        mutex_ (f.mutex_)
    {
      //ni_grabber_.stop ();
      boost::function<void (const CloudConstPtr&)> fn = boost::bind (&GrabberTask::cloudCallback, this, _1);
      boost::signals2::connection c = ni_grabber_->registerCallback (fn);
      ni_grabber_->start ();
      printf ("Copied GrabberTask!\n");
    }
    

    void
    cloudCallback (const CloudConstPtr& cloud)
    {
      tbb::spin_mutex::scoped_lock lock (*mutex_);
      cloud_ = cloud;
      (*updated_) = true;
      printf ("Grabber cloud callback! %d %d\n", cloud->points.size (), cloud_->points.size ());
    }
    
    CloudConstPtr
    operator () (tbb::flow_control& fc) const
    {
      bool ready = false;
      tbb::tick_count::interval_t sleep_time (0.0001);
      printf ("in operator grabbertask\n");
      // Try to get data, until it is available
      // Note: life would be easier if we could use boost::atomic for such a flag
      while (!ready)
      {
        if (mutex_->try_lock ())
        {
          //printf ("updated_: %d\n", updated_);
          if (*updated_)
          {
            printf ("UPDATED!\n");
            printf ("Cloud has %d\n", cloud_->points.size ());
            (*updated_) = false;
            CloudConstPtr ret_cloud (cloud_);
            mutex_->unlock ();
            return (ret_cloud);
          }
          else
          {
            //printf ("not yet updated\n");
            // yield if there isn't new data yet
            mutex_->unlock ();
            //tbb::this_tbb_thread::yield ();
            tbb::this_tbb_thread::sleep (sleep_time);
          }
        }
        else
        {
          // yield if we couldn't get the mutex
          //mutex_.unlock ();
          printf ("couldn't get mutex\n");
          tbb::this_tbb_thread::sleep (sleep_time);
          //tbb::this_tbb_thread::yield ();
        }
      }
    }
};

// A task for publishing pipeline results
template <typename PointT>
class PublishTask
{
  protected:
    mutable double prev_time;
    
  public:
    PublishTask ()
    {
      prev_time = pcl::getTime ();
    }

    PublishTask (const PublishTask<PointT>& f) 
    {}
    
    void
    operator () (CloudConstPtr cloud) const
    {
      printf ("Got cloud with %d\n", cloud->points.size ());
      double time = pcl::getTime ();
      std::cout << "Time :" << double(time - prev_time) << std::endl;
      prev_time = time;
    }  
};

template class PublishTask<pcl::PointXYZRGBA>;
template class GrabberTask<pcl::PointXYZRGBA>;

int
main (int argc, char** argv)
{
  // boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ> > cloud1 (new pcl::PointCloud<pcl::PointXYZ>());
  // boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ> > cloud2 (new pcl::PointCloud<pcl::PointXYZ>());
  // boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ> > cloud3 (new pcl::PointCloud<pcl::PointXYZ>());  

  // pcl::PointXYZ pt1 (0.0, 0.0, 1.0);
  // pcl::PointXYZ pt2 (0.0, 0.0, 2.0);
  // pcl::PointXYZ pt3 (0.0, 0.0, 3.0);

  // cloud1->points.push_back (pt1);
  // cloud2->points.push_back (pt2);
  // cloud3->points.push_back (pt3);
  
  // cloud2 = cloud1;
  // printf ("cloud1: %lf\n", cloud1->points[0].z);// 1.0
  // printf ("cloud2: %lf\n", cloud2->points[0].z);// 1.0
  // cloud1 = cloud3;
  // printf ("cloud1: %lf\n", cloud1->points[0].z);//3.0
  // printf ("cloud2: %lf\n", cloud2->points[0].z);//1.0
  // cloud1.reset ();
  // printf ("cloud2: %lf\n", cloud2->points[0].z);//1.0

  //tbb::pipeline pipeline;
  GrabberTask<pcl::PointXYZRGBA> grabber_task;
  //pipeline.add_filter (grabber_task);

  PublishTask<pcl::PointXYZRGBA> publish_task;

  tbb::filter_t<void, CloudConstPtr> grabber_filter (tbb::filter::serial_in_order, grabber_task);
  tbb::filter_t<CloudConstPtr, void> publish_filter (tbb::filter::serial_in_order, publish_task);

  tbb::filter_t<void, void> filter_chain = grabber_filter & publish_filter;
  tbb::parallel_pipeline (3, filter_chain);
  

  // tbb::parallel_pipeline (3,
  //                         tbb::make_filter<void, CloudPtr> (tbb::filter::serial_in_order, grabber_task ()) &
  //                         tbb::make_filter<CloudPtr, void> (tbb::filter::serial_in_order, publish_task ())
  //                         );
  
  
  
  return (0);
}
