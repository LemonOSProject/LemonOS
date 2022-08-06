# Mark modules as relocatable
add_link_options(-r)

add_executable(ext2fs.sys Ext2/Main.cpp)
add_executable(pcaudio.sys PCAudio/Main.cpp)
add_executable(e1k.sys Intel8254x/Main.cpp)

set(TEST_SRC
    TestModule/Main.cpp
    TestModule/StringTest.cpp
    TestModule/Threading.cpp
)
add_executable(testmodule.sys ${TEST_SRC})