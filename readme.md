# 说明

工具原用于内部的一个压力测试工具，完善了一下，现在开放全部源代码发布出来，希望对您有些作用。

## 工具有什么用

该工具是用于网络调试的工具集，包括了串口(RS232/RS485/RS422 ...)、以太网（TCP/UDP）调试功能。可收发数据，或进行转发数据。可用于模拟各类TCP、UDP、串口、websocket 服务端与客户

## 如何编译

**编译器：**
该工具使用 VS2017 编译  
**依赖第三方库：**  

+ boost 1.68.0  
  
**编译配置：**  

+ “项目配置-> C++ -> 附加包含目录” 添加boost 1.68.0 包含目录
+ “项目配置-> 链接器 -> 附加库目录” 添加boost 1.68.0 库输出目录

## 屏幕截图

![支持作者](https://github.com/Zhou-zhi-peng/NetDebugger/blob/main/Screen/191951.png?raw=true)

![支持作者](https://github.com/Zhou-zhi-peng/NetDebugger/blob/main/Screen/192119.png?raw=true)

![支持作者](https://github.com/Zhou-zhi-peng/NetDebugger/blob/main/Screen/192618.png?raw=true)

## 问答

**为什么使用 MFC？**  
单纯的就是想做成单文件绿色版可执行文件，目前成熟的UI框架看下来,还就它比较合适。  

**转发功能有什么用？**  
转发使用场景  
如果希望监听两个设备（或客户端与服务端）之间的通信，同时还希望可以模拟客户端或服务端发送数据，则需要用到转发。

**多并发客户端是什么？**  
该类客户端主要用于同时生成大量客户端连接到服务器，并进行数据收发，用以对服务器端进行压力测试。

**不想编译BOOST库，如何直接使用？**  
直接下载Bin目录中的EXE文件就可以直接使用。  
如果报应用程序配置不正确，请安装VS2017 C++ 运行时库，也在Bin目录中可以直接下载。  
软件下载页面：<https://github.com/Zhou-zhi-peng/NetDebugger/releases>
运行时下载地址：
X64：<https://aka.ms/vs/17/release/vc_redist.x64.exe>
X86：<https://aka.ms/vs/17/release/vc_redist.x86.exe>

## 支持&捐赠

如果你觉得该软件对你有帮助，想支持一下作者，可扫码向作者进行捐赠，如果觉得没什么用，也可以向作者提出你的意见： <https://github.com/Zhou-zhi-peng/NetDebugger/issues/new>

![支持作者](https://github.com/Zhou-zhi-peng/NetDebugger/blob/main/85347.png?raw=true)
