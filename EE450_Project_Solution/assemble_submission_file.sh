username=zongjian
session=1
folder=submit_temp
result=ee450_${username}_session${session}.tar
rm -f ${result}.gz
rm -rf $folder
mkdir $folder
cp README $folder
cp Makefile $folder
cp Common/common.hpp $folder
cp Client/client.cpp $folder
cp MainServer/aws.cpp $folder
cp ServerA/serverA.cpp $folder
cp ServerB/serverB.cpp $folder
cd $folder
tar cvf $result *
gzip $result
mv ${result}.gz ..
cd ..
rm -rf $folder