plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val repoRoot = rootDir.parentFile

// The GUI and the config files are the repository's own, packaged as assets; the service copies them
// to the app's folder on start. Nothing here is a second copy to keep in step.
val copyAssets = tasks.register<Copy>("copyKeymapperAssets") {
    into(layout.buildDirectory.dir("generated/keymapper-assets"))
    from(File(repoRoot, "web")) { into("web") }
    from(File(repoRoot, "config/actions.android.txt"))
    from(File(repoRoot, "config/mappings.txt")) { rename { "mappings.default.txt" } }
}

android {
    namespace = "io.keymapper"
    compileSdk = 34

    defaultConfig {
        applicationId = "io.keymapper"
        minSdk = 28  // std::filesystem in the NDK, and the lock-screen and screenshot global actions
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
        ndk { abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64") }
        externalNativeBuild { cmake { arguments += listOf("-DANDROID_STL=c++_shared") } }
    }

    externalNativeBuild { cmake { path = File(repoRoot, "CMakeLists.txt") } }

    sourceSets.getByName("main").assets.srcDir(layout.buildDirectory.dir("generated/keymapper-assets"))

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

tasks.configureEach {
    if (name.startsWith("merge") && name.endsWith("Assets")) dependsOn(copyAssets)
}
