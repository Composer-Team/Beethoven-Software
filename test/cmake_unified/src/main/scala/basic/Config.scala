package basic

import beethoven._
import beethoven.Platforms.SimulationPlatform

/**
 * Minimal accelerator configuration for testing the unified CMake build system.
 * Uses the existing MyAccelerator with SimulationPlatform.
 *
 * Run with: sbt "runMain basic.CMakeUnifiedTestBuild"
 */
object CMakeUnifiedTestBuild
    extends BeethovenBuild(
      new AcceleratorConfig(
        List(
          AcceleratorSystemConfig(
            nCores = 1,
            name = "TestSystem",
            moduleConstructor = ModuleBuilder(p => new MyAccelerator(4)(p)),
            memoryChannelConfig = List(
              ReadChannelConfig("vec_in", dataBytes = 4),
              WriteChannelConfig("vec_out", dataBytes = 4)
            )
          )
        )
      ),
      buildMode = BuildMode.Simulation,
      platform = new KriaPlatform()
    )
