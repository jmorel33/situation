set confirm off
file C:/Users/User/Desktop/hobby/_kiro/situation/build/tests/sit_test_vulkan.exe
run --module core --headless
bt 8
info symbol $pc
frame 2
info line
