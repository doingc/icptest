#include <iostream>
#include <string>
#include <pcl/io/ply_io.h> //ply格式读取
#include <pcl/io/pcd_io.h> //pcd格式读取
#include <pcl/point_types.h> //PCL中支持的点类型头文件
#include <pcl/registration/icp.h>
#include <pcl/visualization/pcl_visualizer.h>  //可视化头文件
#include <pcl/console/time.h>   // TicToc 计时

typedef pcl::PointXYZ PointT;
typedef pcl::PointCloud<PointT> PointCloudT;

bool next_iteration = false;

void
print4x4Matrix(const Eigen::Matrix4d& matrix)
{
    printf("Rotation matrix :\n");
    printf("    | %6.3f %6.3f %6.3f | \n", matrix(0, 0), matrix(0, 1), matrix(0, 2));
    printf("R = | %6.3f %6.3f %6.3f | \n", matrix(1, 0), matrix(1, 1), matrix(1, 2));
    printf("    | %6.3f %6.3f %6.3f | \n", matrix(2, 0), matrix(2, 1), matrix(2, 2));
    printf("Translation vector :\n");
    printf("t = < %6.3f, %6.3f, %6.3f >\n\n", matrix(0, 3), matrix(1, 3), matrix(2, 3));
}

void
keyboardEventOccurred(const pcl::visualization::KeyboardEvent& event,
                      void* nothing)//使用空格键来增加迭代次数，并更新显示
{
    if (event.getKeySym() == "space" && event.keyDown())
        next_iteration = true;
}

int
main(int argc,
     char* argv[])
{
    // 将会出现的三个点云模型
    PointCloudT::Ptr cloud_in(new PointCloudT);  // （原始点云）Original point cloud
    PointCloudT::Ptr cloud_tr(new PointCloudT);  // （位姿变化点云）Transformed point cloud
    PointCloudT::Ptr cloud_icp(new PointCloudT);  // （ICP迭代输出点云）ICP output point cloud


    int iterations = 1;  // 默认的ICP迭代次数

    pcl::console::TicToc time;
    time.tic();

    //读取ply文件
    if (pcl::io::loadPLYFile<pcl::PointXYZ>("E:/LX/PLC/点云数据/dragon_stand/dragon_recon.tar/dragon_recon/dragon_vrip_res2.ply", *cloud_in) == -1)
    {
        PCL_ERROR("Couldn't read file1 \n");
        return (-2);
    }
    std::cout << "Loaded " << cloud_in->size() << " data points from file1" << std::endl;

    if (pcl::io::loadPLYFile<pcl::PointXYZ>("E:/LX/PLC/点云数据/dragon_stand/dragon_recon.tar/dragon_recon/dragon_vrip_res2.ply", *cloud_icp) == -1)
    {
        PCL_ERROR("Couldn't read file1 \n");
        return (-2);
    }
    std::cout << "Loaded " << cloud_icp->size() << " data points from file1" << std::endl;

    // 定义旋转矩阵和平移向量Matrix4d是4*4的矩阵
    Eigen::Matrix4d transformation_matrix = Eigen::Matrix4d::Identity();

    // 旋转矩阵 (请参考 https://en.wikipedia.org/wiki/Rotation_matrix)
    double theta = M_PI / 8;  // // 旋转的角度用弧度的表示方法
    transformation_matrix(0, 0) = std::cos(theta);
    transformation_matrix(0, 1) = -sin(theta);
    transformation_matrix(1, 0) = sin(theta);
    transformation_matrix(1, 1) = std::cos(theta);

    // 平移向量在X,Y,Z方向的位移 (meters)
    transformation_matrix(0, 3) = 1;
    transformation_matrix(1, 3) = 2;
    transformation_matrix(2, 3) = 4;

    // 在终端显示转换矩阵
    std::cout << "Applying this rigid transformation to: cloud_in -> cloud_icp" << std::endl;
    print4x4Matrix(transformation_matrix);

    // 执行点云转化
    pcl::transformPointCloud(*cloud_icp, *cloud_icp, transformation_matrix);
    *cloud_tr = *cloud_icp;  // 备份cloud_icp赋值给cloud_tr为后期使用

    // icp算法配准
    time.tic();
    pcl::IterativeClosestPoint<PointT, PointT> icp;
    icp.setMaximumIterations(iterations); //设置要执行的初始迭代次数（默认值为1）
    icp.setInputSource(cloud_icp);//设置输入点云
    icp.setInputTarget(cloud_in); //设置目标点云（输入点云进行仿射变换，得到目标点云）
    icp.align(*cloud_icp);//匹配后源点云
    icp.setMaximumIterations(1);  // 设置为1以便下次调用
    std::cout << "Applied " << iterations << " ICP iteration(s) in " << time.toc() << " ms" << std::endl;

    if (icp.hasConverged())   //输出变换矩阵的适合性评估，检查ICP算法是否收敛；否则退出程序。如果成功，我们将转换矩阵存储在4x4矩阵中，然后打印刚性矩阵转换
    {
        std::cout << "\nICP has converged, score is " << icp.getFitnessScore() << std::endl;
        std::cout << "\nICP transformation " << iterations << " : cloud_icp -> cloud_in" << std::endl;
        transformation_matrix = icp.getFinalTransformation().cast<double>();
        print4x4Matrix(transformation_matrix);
    }
    else
    {
        PCL_ERROR("\nICP has not converged.\n");
        return (-1);
    }

    // //可视化
    pcl::visualization::PCLVisualizer viewer("ICP demo");
    // 创建两个独立垂直观察视点
    int v1(0);

    int v2(1);
    viewer.createViewPort(0.0, 0.0, 0.5, 1.0, v1);
    viewer.createViewPort(0.5, 0.0, 1.0, 1.0, v2);

    // 定义显示的颜色信息
    float bckgr_gray_level = 0.0;  // 黑色
    float txt_gray_lvl = 1.0 - bckgr_gray_level;

    // 原始的点云设置为白色的
    pcl::visualization::PointCloudColorHandlerCustom<PointT> cloud_in_color_h(cloud_in, (int)255 * txt_gray_lvl, (int)255 * txt_gray_lvl,
                                                                              (int)255 * txt_gray_lvl);
    viewer.addPointCloud(cloud_in, cloud_in_color_h, "cloud_in_v1", v1);
    viewer.addPointCloud(cloud_in, cloud_in_color_h, "cloud_in_v2", v2);

    //  转换后的点云显示为绿色
    pcl::visualization::PointCloudColorHandlerCustom<PointT> cloud_tr_color_h(cloud_tr, 20, 180, 20);
    viewer.addPointCloud(cloud_tr, cloud_tr_color_h, "cloud_tr_v1", v1);

    //  ICP配准后的点云为红色
    pcl::visualization::PointCloudColorHandlerCustom<PointT> cloud_icp_color_h(cloud_icp, 180, 20, 20);
    viewer.addPointCloud(cloud_icp, cloud_icp_color_h, "cloud_icp_v2", v2);

    // 加入文本的描述在各自的视口界面
    viewer.addText("White: Original point cloud\nGreen: Matrix transformed point cloud", 10, 15, 16, txt_gray_lvl, txt_gray_lvl, txt_gray_lvl, "icp_info_1", v1);
    viewer.addText("White: Original point cloud\nRed: ICP aligned point cloud", 10, 15, 16, txt_gray_lvl, txt_gray_lvl, txt_gray_lvl, "icp_info_2", v2);

    std::stringstream ss;  //需要使用字符串流ss来将整数迭代转换为字符串，在终端显示迭代次数
    ss << iterations;
    std::string iterations_cnt = "ICP iterations = " + ss.str();
    viewer.addText(iterations_cnt, 10, 60, 16, txt_gray_lvl, txt_gray_lvl, txt_gray_lvl, "iterations_cnt", v2);


    viewer.setBackgroundColor(bckgr_gray_level, bckgr_gray_level, bckgr_gray_level, v1);
    viewer.setBackgroundColor(bckgr_gray_level, bckgr_gray_level, bckgr_gray_level, v2);

    //  设置相机的坐标和方向（在终端初始观察的位置）
    viewer.setCameraPosition(-3.68332, 2.94092, 5.71266, 0.289847, 0.921947, -0.256907, 0);
    viewer.setSize(1280, 1024);  //可视化窗口的大小

    // 配准按键回调函数 :
    viewer.registerKeyboardCallback(&keyboardEventOccurred, (void*)NULL);

    //显示可视化终端
    while (!viewer.wasStopped())
    {
        viewer.spinOnce();

        // 按下空格键 :
        if (next_iteration)
        {
            // The Iterative Closest Point algorithm（ICP算法）
            time.tic();
            icp.align(*cloud_icp);
            std::cout << "Applied 1 ICP iteration in " << time.toc() << " ms" << std::endl;

            if (icp.hasConverged())
            {
                printf("\033[11A");  // Go up 11 lines in terminal output.
                printf("\nICP has converged, score is %+.0e\n", icp.getFitnessScore());
                std::cout << "\nICP transformation " << ++iterations << " : cloud_icp -> cloud_in" << std::endl;
                transformation_matrix *= icp.getFinalTransformation().cast<double>();  // WARNING /!\ This is not accurate! For "educational" purpose only!
                print4x4Matrix(transformation_matrix);  // // 打印原始位置与现在位置的矩阵变换

                ss.str("");
                ss << iterations;
                std::string iterations_cnt = "ICP iterations = " + ss.str();
                viewer.updateText(iterations_cnt, 10, 60, 16, txt_gray_lvl, txt_gray_lvl, txt_gray_lvl, "iterations_cnt");
                viewer.updatePointCloud(cloud_icp, cloud_icp_color_h, "cloud_icp_v2");
            }
            else
            {
                PCL_ERROR("\nICP has not converged.\n");
                return (-1);
            }
        }
        next_iteration = false;
    }
    return (0);
}