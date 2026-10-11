$o = "build/pc"; New-Item -ItemType Directory -Force $o | Out-Null
$cc = "lib/CubeCore/src"
g++ -O2 -std=c++17 -I $cc -I lib/Robot/src tools/pc/pose_sim/pose_sim.cpp $cc/CubeState.cpp -o $o/pose_sim.exe
g++ -O2 -std=c++17 -I lib/Protocol/src tools/pc/proto_test/proto_test.cpp -o $o/proto_test.exe
g++ -O2 -std=c++17 -I $cc tools/pc/gen_tables/gen.cpp $cc/Coords.cpp $cc/CubieCube.cpp $cc/CubeState.cpp -o $o/gen_tables.exe