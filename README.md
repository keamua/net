# net
- computer_net_exp
- 南开计网的第三次大实验，要求基于udp实现可靠文件传输。
- 2020/12/9 目前是从网上找到的能够实现客户端与服务器端传输文件的代码，将进行可靠传输的设计
- 2020/12/11 凌晨进度是，面向连接和差错检测（基本和没做一样），进行确认重传时，卡在设计计时器了。滑动窗口药丸，无所谓的。寄了
- 2020/12/11 中午，能正确的传输和写入打包好的文件了，确认重传逻辑上还可以，没敢试
- 2020/12/17 晚上，从新写了一个能够，在窗口中实现文件传输
