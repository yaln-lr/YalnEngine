    
    #include "../include/yalncamera.h"
    glm::mat4 YalnCamera::getMatrix()
    {
        return getProjectionMatrix()*getViewMatrix();
    };