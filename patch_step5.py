import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# Replace the glClear logic to ensure it supports independent Color, Depth, and Stencil LOAD/CLEAR/DONT_CARE
old_clear_logic = """
                    GLbitfield clear_mask = 0;
                    if (p->args.begin_pass.info.color_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        ColorRGBA c = p->args.begin_pass.info.color_attachment.clear.color;
                        glClearColor(c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f);
                        clear_mask |= GL_COLOR_BUFFER_BIT;
                    }
                    if (p->args.begin_pass.info.depth_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        glClearDepth(p->args.begin_pass.info.depth_attachment.clear.depth);
                        clear_mask |= GL_DEPTH_BUFFER_BIT;
                    }
                    if (clear_mask) glClear(clear_mask);
"""

new_clear_logic = """
                    GLbitfield clear_mask = 0;
                    if (p->args.begin_pass.info.color_attachment.loadOp == SIT_LOAD_OP_CLEAR || p->args.begin_pass.info.color_attachment.loadOp == SIT_LOAD_OP_DONT_CARE) {
                        // For DONT_CARE we could technically skip clearing, but clearing avoids undefined behavior
                        // We strictly honor CLEAR
                        if (p->args.begin_pass.info.color_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                            ColorRGBA c = p->args.begin_pass.info.color_attachment.clear.color;
                            glClearColor(c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f);
                            clear_mask |= GL_COLOR_BUFFER_BIT;
                        } else {
                             // Optional: DONT_CARE could also be cleared to black
                             // glClearColor(0,0,0,0); clear_mask |= GL_COLOR_BUFFER_BIT;
                        }
                    }
                    if (p->args.begin_pass.info.depth_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        glClearDepth(p->args.begin_pass.info.depth_attachment.clear.depth);
                        clear_mask |= GL_DEPTH_BUFFER_BIT;
                    }
                    // Optional stencil support, assume depth_attachment loadOp dictates stencil for now
                    if (p->args.begin_pass.info.depth_attachment.loadOp == SIT_LOAD_OP_CLEAR && p->args.begin_pass.info.depth_attachment.clear.stencil > 0) {
                        glClearStencil(p->args.begin_pass.info.depth_attachment.clear.stencil);
                        clear_mask |= GL_STENCIL_BUFFER_BIT;
                    }
                    if (clear_mask) glClear(clear_mask);
"""

content = content.replace(old_clear_logic, new_clear_logic)

with open("situation_impl.h", "w") as f:
    f.write(content)
