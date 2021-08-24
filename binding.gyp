{
    "targets": [
        {
            "target_name": "aminonative",
            "sources": [
                "src/base.cpp",
                "src/base_js.cpp",
                "src/base_weak.cpp",

                "src/fonts/vector.c",
                "src/fonts/vertex-buffer.c",
                "src/fonts/vertex-attribute.c",
                "src/fonts/texture-atlas.c",
                "src/fonts/texture-font.c",
                "src/fonts/utf8-utils.c",
                "src/fonts/distance-field.c",
                "src/fonts/edtaa3func.c",
                "src/fonts/shader.c",
                "src/fonts/mat4.c",
                "src/fonts.cpp",

                "src/images.cpp",

                "src/videos.cpp",

                "src/shaders.cpp",
                "src/renderer.cpp",
                "src/mathutils.cpp"
            ],
            "include_dirs": [
                "<!(node -e \"require('nan')\")",
                "src/",
                "src/fonts/",
                "src/images/"
            ],
            "cflags": [
                "-Wall"
            ],
            "cxxflags": [
                "-std=c++14"
            ],
            'conditions': [
                # macOS
                [ 'OS == "mac"', {
                    "include_dirs": [
                        " <!@(pkg-config --cflags freetype2)",
                        " <!@(pkg-config --cflags glfw3)"
                    ],
                    "libraries": [
                        " <!@(pkg-config --libs glfw3)",
                        '-framework OpenGL',
                        '-framework OpenCL',
                        '-framework IOKit',
                        '<!@(pkg-config --libs freetype2)',
                        '-ljpeg',
                        '-lpng',
                        '-lavcodec',
                        '-lavformat',
                        '-lavutil',
                        '-lswscale'
                    ],
                    "sources": [
                        "src/mac.cpp"
                    ],
                    "defines": [
                        "MAC",
                        "GLFW_NO_GLU",
                        "GLFW_INCLUDE_GL3"

                        # VAO not working
                        #"FREETYPE_GL_USE_VAO"
                    ],
                    "xcode_settings": {
                        "OTHER_CPLUSPLUSFLAGS": [
                            "-std=c++14",
                            "-stdlib=libc++"
                        ],
                        "OTHER_LDFLAGS": [
                            "-stdlib=libc++"
                        ],
                        "MACOSX_DEPLOYMENT_TARGET": "10.7"
                    }
                }],

                # Raspberry Pi
                [ 'OS == "linux"', {
					"conditions" : [
	                    [ "target_arch == 'arm'", {
		                    "sources": [
                                # OMX
                                "src/ilclient/ilclient.c",
                                "src/ilclient/ilcore.c",
                                # base
		                        "src/rpi.cpp",
                                "src/rpi_video.cpp"
		                    ],
		                    "libraries": [
		                        '<!@(pkg-config --libs freetype2)',
                                '-ljpeg',
                                '-lpng',
                                '-lavcodec',
                                '-lavformat',
                                '-lavutil',
                                '-lswscale'
		                    ],
                            'variables': {
                                'rpi_model': '"<!@(awk \'/^Revision/ {sub(\"^1000\", \"\", $3); print $3}\' /proc/cpuinfo)"',
                                'is_rpi_4': '<!(cat /sys/firmware/devicetree/base/model | { grep -c "Pi 4" || true; })'
                            },
                            'actions': [{
                                # output RPi model
                                'action_name': 'build_info',
                                'action': [
                                    'echo',
                                    'RPi model: <(rpi_model); Pi 4: <(is_rpi_4)'
                                ],
                                'inputs': [],
                                'outputs': [ "src/rpi.cpp" ]
                            }],
                            # OS specific libraries
                            'conditions': [
                                # RPi 4
                                [ '<(is_rpi_4) == 1', {
                                    "include_dirs": [
                                        " <!@(pkg-config --cflags libdrm)"
                                    ],
                                    'libraries': [
                                        "-lGL",
		                                "-lEGL",
                                        '<!@(pkg-config --libs libdrm)',
                                        '-lgbm'
                                    ],
                                    'defines': [
                                        # RPi 4 support
                                        'RPI_BUILD="RPI 4 (Mesa, DRM, GBM)"',
                                        "EGL_GBM"
                                    ]
                                }, {
                                    # RPi 3
                                    'conditions': [
                                        [ '"<!@(lsb_release -c -s)" == "jessie"', {
                                            # RPi 3 (Jessie 8.x)
                                            'libraries': [
                                                # OpenGL
                                                "-lGLESv2",
                                                "-lEGL",
                                                # VideoCore
                                                "-L/opt/vc/lib/",
                                                "-lbcm_host",
                                                "-lopenmaxil",
                                                "-lvcos",
                                                "-lvchiq_arm"
                                            ],
                                            'defines': [
                                                # RPi 3
                                                'RPI_BUILD="RPI 3 (Jessie, Dispmanx, OMX)"',
                                                "EGL_DISPMANX"
                                            ]
                                        }, {
                                            # RPi 3 (Stretch and newer; >= 9.x)
                                            'libraries': [
                                                # OpenGL
                                                "-lbrcmGLESv2",
                                                "-lbrcmEGL",
                                                # VideoCore
                                                "-L/opt/vc/lib/",
                                                "-lbcm_host",
                                                "-lopenmaxil",
                                                "-lvcos",
                                                "-lvchiq_arm"
                                            ],
                                            'defines': [
                                                # RPi 3
                                                'RPI_BUILD="RPI 3 (Dispmanx, OMX)"',
                                                "EGL_DISPMANX"
                                            ]
                                        }]
                                    ]
                                }],
                            ],
		                    "defines": [
		                        "RPI"
		                    ],
		                    "include_dirs": [
                                # VideoCore
		                        "/opt/vc/include/",
                                "/opt/vc/include/IL/",
		                        "/opt/vc/include/interface/vcos/pthreads",
		                        "/opt/vc/include/interface/vmcs_host/linux",
                                "/opt/vc/include/interface/vchiq/",
                                # Freetype
                                "/usr/include/freetype2",
		                        '<!@(pkg-config --cflags freetype2)'
		                    ],
                            "cflags": [
                                # VideoCore
                                "-DHAVE_LIBOPENMAX=2",
                                "-DOMX",
                                "-DOMX_SKIP64BIT",
                                "-DUSE_EXTERNAL_OMX",
                                "-DHAVE_LIBBCM_HOST",
                                "-DUSE_EXTERNAL_LIBBCM_HOST",
                                "-DUSE_VCHIQ_ARM",
                                # get stack trace on ARM
                                "-funwind-tables",
                                "-rdynamic",
                                # NAN warnings (remove later; see https://github.com/nodejs/nan/issues/807)
                                "-Wno-cast-function-type"

                            ],
                            "cflags_cc": [
                                # NAN weak reference warning (remove later)
                                "-Wno-class-memaccess"
                            ]
		                }]
		            ]
                }]
            ]
        },
        {
            "target_name": "action_after_build",
            "type": "none",
            "dependencies": [ "<(module_name)" ],
            "copies": [{
                "files": [ "<(PRODUCT_DIR)/<(module_name).node" ],
                "destination": "<(module_path)"
            }]
        }
    ]
}
