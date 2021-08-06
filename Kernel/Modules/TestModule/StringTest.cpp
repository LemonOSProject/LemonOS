#include <String.h>

#include <Logging.h>

int StringTest() {
    Log::Info("[TestModule] Running String Test...");

    String test = "tesst";
    if (test.Compare("tesst")) {
        Log::Warning("Failed Test 0 with Result '%s'", test.Data());
    }

    String dog("dog");
    String cat("cat");

    if ((dog + cat).Compare("dogcat")) {
        Log::Warning("Failed Test 1 with Result '%s'", (dog + cat).Data());
    }

    test = std::move(dog);
    test += cat;
    if (test.Compare(String("dog") + cat)) {
        Log::Warning("Failed Test 2 with Result '%s'", test.Data());
    }

    return 0;
}
