/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2010-2012  hkrn                                    */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the MMDAI project team nor the names of     */
/*   its contributors may be used to endorse or promote products     */
/*   derived from this software without specific prior written       */
/*   permission.                                                     */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

#ifndef VPVL2_ILIGHT_H_
#define VPVL2_ILIGHT_H_

#include "vpvl2/Common.h"

namespace vpvl2
{

class IMotion;

class VPVL2_API ILight
{
public:
    virtual ~ILight() {}

    virtual Vector3 color() const = 0;
    virtual Vector3 direction() const = 0;
    virtual Vector3 depthTextureSize() const = 0;
    virtual void *depthTexture() const = 0;
    virtual bool hasFloatTexture() const = 0;
    virtual bool isToonEnabled() const = 0;
    virtual bool isSoftShadowEnabled() const = 0;
    virtual IMotion *motion() const = 0;
    virtual void setColor(const Vector3 &value) = 0;
    virtual void setDirection(const Vector3 &value) = 0;
    virtual void setDepthTextureSize(const Vector3 &value) = 0;
    virtual void setMotion(IMotion *value) = 0;
    virtual void setDepthTexture(void *value) = 0;
    virtual void setHasFloatTexture(bool value) = 0;
    virtual void setToonEnable(bool value) = 0;
    virtual void setSoftShadowEnable(bool value) = 0;
    virtual void copyFrom(const ILight *value) = 0;
    virtual void resetDefault() = 0;
};

} /* namespace vpvl2 */

#endif
