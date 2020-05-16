# memcached slo measure

此工具是对 memcached 产生批量 get 或/和 set 请求，从而进行与 SLO 有关的平均延迟、尾端延迟、吞吐量等参数的测量。

## building

```
sudo apt-get update
sudo apt-get install libevent-dev gengetopt

mkdir build
cd build
cmake ..
make
```

## usage

简单使用

```
memcached &
./slo_measure -s "localhost"
```

更多参数选项通过`-h`查看

```
./slo_measure -h
```

## extension

可以通过继承抽象类`Protocol`扩展至对更多软件的 SLO 有关参数的测量。