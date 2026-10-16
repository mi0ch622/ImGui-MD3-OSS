plugins {
    id("com.android.application")
}

android {
    namespace = "com.u3dexample"
    compileSdk = 33

    // 使用已安装的 NDK 27.3.13750724 (r27d), 阻止 AGP 自动下载默认 NDK
    ndkVersion = "27.1.12297006"

    defaultConfig {
        applicationId = "com.u3dexample"
        minSdk = 21
        targetSdk = 33
        versionCode = 1
        versionName = "1.0"

        vectorDrawables {
            useSupportLibrary = true
        }

        // 只编 arm64
        ndk {
            abiFilters.add("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                cppFlags("")
                arguments.add("-DANDROID_STL=c++_static")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    buildFeatures {
        viewBinding = true
    }

    flavorDimensions += "app"

    productFlavors {
        create("full") {
            dimension = "app"
            applicationId = "com.u3dexample"
        }
        create("proctest") {
            dimension = "app"
            applicationId = "com.u3dexample.proctestapp"
            versionCode = 1
            versionName = "1.0-test"
        }
        create("range") {
            dimension = "app"
            applicationId = "com.u3dexample.practice"
            versionCode = 1
            versionName = "1.0-range"
        }
    }


    externalNativeBuild {
        cmake {
            path(file("src/main/cpp/CMakeLists.txt"))
        }
    }
}

dependencies {

    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("com.google.android.material:material:1.9.0")
}
