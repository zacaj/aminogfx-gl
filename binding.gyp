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
                "-Wall",
            ],
            "cxxflags": [
                "-std=c++11"
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
                        "src/mac.cpp",
                    ],
                    "defines": [
                        "MAC",
                        "GLFW_NO_GLU",
                        "GLFW_INCLUDE_GL3",

                        # VAO not working
                        #"FREETYPE_GL_USE_VAO"
                    ],
                    "xcode_settings": {
                        "OTHER_CPLUSPLUSFLAGS": [
                            "-std=c++11",
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
	                    [ "target_arch=='arm'", {
		                    "sources": [
                                "src/ilclient/ilclient.c",
                                "src/ilclient/ilcore.c",
		                        "src/rpi.cpp",
                                "src/rpi_video.cpp"
		                    ],
		                    "libraries": [
		                        "-L/opt/vc/lib/",
                                "-lbcm_host",
                                "-lopenmaxil",
                                "-lvcos",
                                "-lvchiq_arm",
		                        '<!@(pkg-config --libs freetype2)',
                                "-ljpeg",
                                "-lpng",
                                '-lavcodec',
                                '-lavformat',
                                '-lavutil',
                                '-lswscale'
		                    ],
                            # OS specific libraries
                            'conditions': [
                                # Buster (10)
                                # TODO
                                [ '<!@(lsb_release -r -s) == 10', {
                                    'libraries': [
                                        "-lbrcmGLESv2",
		                                "-lbrcmEGL"
                                    ],
                                    'defines': [
                                        "EGL_GBM"
                                    ]
                                }],
                                # Stretch (9)
                                [ '<!@(lsb_release -r -s) == 9', {
                                    'libraries': [
                                        "-lbrcmGLESv2",
		                                "-lbrcmEGL"
                                    ],
                                    'defines': [
                                        "EGL_DISPMANX"
                                    ]
                                }],
                                # Jessie (8)
                                [ '<!@(lsb_release -r -s) == 8', {
                                    'libraries': [
                                        "-lGLESv2",
		                                "-lEGL",
                                    ],
                                    'defines': [
                                        "EGL_DISPMANX"
                                    ]
                                }]
                            ],
		                    "defines": [
		                        "RPI"
		                    ],
		                    "include_dirs": [
		                        "/opt/vc/include/",
                                "/opt/vc/include/IL/",
		                        "/usr/include/freetype2",
		                        "/opt/vc/include/interface/vcos/pthreads",
		                        "/opt/vc/include/interface/vmcs_host/linux",
                                "/opt/vc/include/interface/vchiq/",
		                        '<!@(pkg-config --cflags freetype2)'
		                    ],
                            "cflags": [
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
                                "-Wno-cast-function-type",

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
