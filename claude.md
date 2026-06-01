# Claude General Rules

* Always follow the style rules as specified in .clang-format in the root node of the repository
* Always verify changes to the code build by running "make" in the project directory (or using the util/dfu.py tool in build-only mode)
* Always verify that changes to the code pass unit-tests by running the unit tests. Instructions for the unit tests are in unit_testing/unit_tests.md
* If adding a new feature to the code, always add (at least one) test to verify its functionality.
* Wherever possible, reuse existing code in CA_Embedded_Libraries. If a relevant usable code-snippet exists in another project directory, do not reference it directly, but consider moving it to CA_Embedded_Libraries.