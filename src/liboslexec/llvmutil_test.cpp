// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>

#include <OSL/llvm_util.h>


typedef int (*IntFuncOfTwoInts)(int, int);

static bool verbose = false;
static bool debug   = false;
static int memtest  = 0;



static void
getargs(int argc, char* argv[])
{
    OIIO::ArgParse ap;
    // clang-format off
    ap.intro("llvmutil_test\n" OIIO_INTRO_STRING);
    ap.usage("llvmutil_test [options]");
    ap.arg("-v", &verbose)
      .help("Verbose output");
    ap.arg("--debug", &debug)
      .help("Debug mode");
    ap.arg("--memtest %d:ITERATIONS", &memtest)
      .help("Memory test mode");
    // clang-format on
    ap.parse_args(argc, (const char**)argv);
}



// This demonstrates the use of LLVM_Util to generate the following
// function on the fly, JIT it, and call it:
//      int myadd (int arg1, int arg2)
//      {
//          return arg1 + arg2;
//      }
//
void
test_int_func()
{
    // Setup
    OSL::pvt::LLVM_Util::PerThreadInfo pti;
    OSL::pvt::LLVM_Util ll(pti);

    // Make a function with prototype:   int myadd (int arg1, int arg2)
    // and make it the current function.
    auto func = ll.make_function("myadd",                            // name
                                 false,                              // fastcall
                                 ll.type_int(),                      // return
                                 { ll.type_int(), ll.type_int() });  // args
    ll.current_function(func);

    // Generate the ops for this function:  return arg1 + arg2
    llvm::Value* arg1 = ll.current_function_arg(0);
    llvm::Value* arg2 = ll.current_function_arg(1);
    llvm::Value* sum  = ll.op_add(arg1, arg2);
    ll.op_return(sum);

    // Optimize it
    ll.setup_optimization_passes(0);
    ll.do_optimize();

    // Print the optimized bitcode
    std::cout << "Generated the following bitcode:\n"
              << ll.bitcode_string(func) << "\n";

    // Ask for a callable function (will JIT on demand)
    IntFuncOfTwoInts myadd = (IntFuncOfTwoInts)ll.getPointerToFunction(func);

    // Call it:
    int result = myadd(13, 29);
    std::cout << "The result is " << result << "\n";
    OIIO_CHECK_EQUAL(result, 42);
}



// This demonstrates the use of LLVM_Util to generate the following
// function on the fly, JIT it, and call it:
//      void myaddv (Vec3 *result, Vec3 *a, float b)
//      {
//          *result = (*a) * b;
//      }
//
void
test_triple_func()
{
    // Setup
    OSL::pvt::LLVM_Util::PerThreadInfo pti;
    OSL::pvt::LLVM_Util ll(pti);

    // Make a function with prototype:   Vec3 myadd (Vec3 *arg1, float arg2)
    // and make it the current function.
    auto func = ll.make_function("myaddv",        // name
                                 false,           // fastcall
                                 ll.type_void(),  // return
                                 { (llvm::Type*)ll.type_triple_ptr(),  // result
                                   (llvm::Type*)ll.type_triple_ptr(),  // arg1
                                   ll.type_float() });                 // arg2
    ll.current_function(func);

    // Generate the ops for this function:  r = a*b
    llvm::Value* rptr = ll.current_function_arg(0);
    llvm::Value* aptr = ll.current_function_arg(1);
    llvm::Value* b    = ll.current_function_arg(2);
    for (int i = 0; i < 3; ++i) {
        llvm::Value* r_elptr = ll.GEP((llvm::Type*)ll.type_triple(), rptr, 0,
                                      i);
        llvm::Value* a_elptr = ll.GEP((llvm::Type*)ll.type_triple(), aptr, 0,
                                      i);
        llvm::Value* product = ll.op_mul(ll.op_load(ll.type_float(), a_elptr),
                                         b);
        ll.op_store(product, r_elptr);
    }
    ll.op_return();

    // Optimize it
    ll.setup_optimization_passes(0);
    ll.do_optimize();

    // Print the optimized bitcode
    std::cout << "Generated the following bitcode:\n"
              << ll.bitcode_string(func) << "\n";

    // Ask for a callable function (will JIT on demand)
    typedef void (*FuncVecVecFloat)(void*, void*, float);
    FuncVecVecFloat f = (FuncVecVecFloat)ll.getPointerToFunction(func);

    // Call it:
    {
        float r[3], a[3] = { 1.0, 2.0, 3.0 }, b = 42.0;
        f(r, a, b);
        std::cout << "The result is " << r[0] << ' ' << r[1] << ' ' << r[2]
                  << "\n";
        OIIO_CHECK_EQUAL(r[0], 42.0);
        OIIO_CHECK_EQUAL(r[1], 84.0);
        OIIO_CHECK_EQUAL(r[2], 126.0);
    }
}



// Make a crazy big function with lots of IR, having prototype:
//      int mybig (int arg1, int arg2);
//
IntFuncOfTwoInts
test_big_func(bool do_print = false)
{
    // Setup
    OSL::pvt::LLVM_Util::PerThreadInfo pti;
    OSL::pvt::LLVM_Util ll(pti);

    // Make a function with prototype:  int myadd (int arg1, int arg2)
    // and make it the current function in the current module.
    auto func = ll.make_function("myadd",                            // name
                                 false,                              // fastcall
                                 ll.type_int(),                      // return
                                 { ll.type_int(), ll.type_int() });  // args
    // Make it the current function and get it ready to accept IR.
    ll.current_function(func);

    // Generate the ops for this function:  return arg1 + arg2
    llvm::Value* arg1 = ll.current_function_arg(0);
    llvm::Value* arg2 = ll.current_function_arg(1);
    llvm::Value* sum  = ll.op_add(arg1, arg2);
    // Additional useless assignments, to bloat code
    for (int i = 0; i < 1000; ++i) {
        sum = ll.op_add(arg1, arg2);
    }
    ll.op_return(sum);

    // Print the optimized bitcode
    // if (do_print)
    //     std::cout << "Generated the following bitcode:\n"
    //               << ll.bitcode_string(func) << "\n";

    ll.setup_optimization_passes(0, true /*targetHost*/);
    ll.do_optimize();

    if (do_print)
        std::cout << "After optimizing:\n" << ll.bitcode_string(func) << "\n";

    // Ask for a callable function (will JIT on demand)
    IntFuncOfTwoInts myadd = (IntFuncOfTwoInts)ll.getPointerToFunction(func);

    // We're done with the module now
    // ll.remove_module (module);

    // Return the function. The callable code should survive the destruction
    // of the LLVM_Util and its resources!
    return myadd;
}



void
test_isa_features()
{
    OSL::pvt::LLVM_Util::PerThreadInfo pti;
    OSL::pvt::LLVM_Util ll(pti);

    ll.detect_cpu_features();

    // Make sure it matches what OIIO's cpuid queries reveal
    OIIO_CHECK_EQUAL(ll.supports_avx(), OIIO::cpu_has_avx());
    OIIO_CHECK_EQUAL(ll.supports_avx2(), OIIO::cpu_has_avx2());
    OIIO_CHECK_EQUAL(ll.supports_avx512f(), OIIO::cpu_has_avx512f());
}



int
main(int argc, char* argv[])
{
    getargs(argc, argv);

    // This dummy object is the "owner" of the memory holding JITed code.
    // It must outlive any LLVM_Util (and its PerThreadInfo).
    OSL::pvt::LLVM_Util::ScopedJitMemoryUser llvm_jit_memory_user;

    test_isa_features();

    // Test simple functions
    test_int_func();
    test_triple_func();

    if (memtest) {
        for (int i = 0; i < memtest; ++i) {
            IntFuncOfTwoInts f = test_big_func(i == 0);
            int r              = f(42, 42);
            OIIO_CHECK_EQUAL(r, 84);
        }
        std::cout << "After " << memtest << " stupid functions compiled:\n";
        std::cout << "   RSS memory = "
                  << OIIO::Strutil::memformat(OIIO::Sysutil::memory_used())
                  << "\n";
    }

    return unit_test_failures;
}
