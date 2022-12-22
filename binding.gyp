{
    'targets': [{
        'target_name': 'aminonative',

        'variables': {
            # flags
            'use_glfw': 0,

            # Note: fails on Parallels (no hardware accelerated driver)
            # Note: not supported by Raspberry Pi
            'use_vaapi': 0,

            # Linux
            'linux_codename': '',
            'is_linux': 0, # exlcuding RPi

            # Raspberry Pi
            'rpi_model': '',
            'is_rpi': 0,
            'is_rpi_4': 0,

            'conditions': [
                # Linux
                [ 'OS == "linux"', {
                    # any
                    'linux_codename': '"<!@(lsb_release -c -s)"', # e.g. 'jessie', 'bullseye'

                    # Raspberry Pi
                    'rpi_model': '"<!@(awk \'/^Revision/ {sub(\"^1000\", \"\", $3); print $3}\' /proc/cpuinfo)"',

                    'conditions': [
                        # evaluate rpi_model again
                        [ '"<!@(awk \'/^Revision/ {sub(\"^1000\", \"\", $3); print $3}\' /proc/cpuinfo)" == ""', {
                            # Linux (excluding RPi)
                            'use_glfw': 1,
                            'is_linux': 1,
                            'is_rpi': 0
                        }, {
                            # Raspberry Pi
                            'is_rpi': 1,

                            # check Pi 4
                            'is_rpi_4': '<!(cat /sys/firmware/devicetree/base/model | { grep -c "Pi 4" || true; })'
                        }],
                    ]
                }],

                # macOS
                [ 'OS == "mac"', {
                    'use_glfw': 1
                }]
            ]
        },

        # report flags
        'actions': [{
            'action_name': 'build_flags',
            'action': [
                'echo',
                'Build flags: OS=<(OS) use_glfw=<(use_glfw) rpi_model=<(rpi_model) is_rpi=<(is_rpi) is_rpi_4=<(is_rpi_4) is_linux=<(is_linux) linux_codename=<(linux_codename)'
            ],
            'inputs': [],
            'outputs': [ 'src/base.cpp' ] # Note: file has to exist
        }],

        # platform independent source files
        "sources": [
            # JS bindings
            "src/base.cpp",
            "src/base_js.cpp",
            "src/base_weak.cpp",

            # font support
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

            # image loaders
            "src/images.cpp",

            # video support
            "src/videos.cpp",

            # rendering
            "src/shaders.cpp",
            "src/renderer.cpp",
            "src/mathutils.cpp"
        ],

        # directories
        "include_dirs": [
            "<!(node -e \"require('nan')\")",
            "src/",
            "src/fonts/",
            "src/edid/",
            # Note: space at beginning needed
            ' <!@(pkg-config --cflags freetype2)'
        ],

        # libraries
        "libraries": [
            '<!@(pkg-config --libs freetype2)',
            '-ljpeg',
            '-lpng',
            '-lavcodec',
            '-lavformat',
            '-lavutil',
            '-lswscale'
        ],

        # compiler flags
        "cflags": [
            "-Wall"
        ],

        "cxxflags": [
            "-std=c++14"
        ],

        # platform specific part
        'conditions': [
            # GLFW library
            [ 'use_glfw == 1', {
                'defines': [
                    # see https://www.glfw.org/docs/latest/build_guide.html#build_macros
                    #cbxx TODO verify
                    'GLFW_INCLUDE_ES2',
                    'GLFW_INCLUDE_GLEXT'
                ],

                'include_dirs': [
                    # Note: space at beginning needed
                    ' <!@(pkg-config --cflags glfw3 gl)'
                ],

                'libraries': [
                    '<!@(pkg-config --libs glfw3 gl)',
                    # cbxx TODO verify -> fails on M1 Ubuntu
                    #'-lGLESv2',
                    #'-lGLES2'
                ],

                'sources': [
                    'src/glfw.cpp'
                ],

                'defines': [
                    #'GLFW_NO_GLU',
                    #'GLFW_INCLUDE_GL3',
                ]
            }],

            # VA-PI
            [
                'use_vaapi == 1', {
                    "defines": [
                        "VAAPI"
                    ]
                }
            ],

            # macOS
            [ 'OS == "mac"', {
                "libraries": [
                    '-framework OpenGL',
                    '-framework OpenCL',
                    '-framework IOKit'
                ],

                "defines": [
                    "MAC",

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

            # Linux (but not RPi)
            [ 'is_linux == 1', {
                "libraries": [
                    '-lGL',
                ],

                "sources": [
                    # EDID
                    "src/edid/edid.c",
                ],

                "defines": [
                    'LINUX',
                    'GL_GLEXT_PROTOTYPES'
                ],

                "cflags": [
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
            }],

            # Raspberry Pi
            [ 'is_rpi == 1', {
                "sources": [
                    # OMX
                    "src/ilclient/ilclient.c",
                    "src/ilclient/ilcore.c",
                    # EDID
                    "src/edid/edid.c",
                    # base
                    "src/rpi.cpp",
                    "src/rpi_input.cpp",
                    "src/rpi_video.cpp"
                ],
                # OS specific libraries
                'conditions': [
                    # RPi 4
                    [ 'is_rpi_4 == 1', {
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
                            [ 'linux_codename == "jessie"', {
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
    },
    {
        "target_name": "action_after_build",
        "type": "none",
        "dependencies": [ "<(module_name)" ],
        "copies": [{
            "files": [ "<(PRODUCT_DIR)/<(module_name).node" ],
            "destination": "<(module_path)"
        }]
    }]
}
