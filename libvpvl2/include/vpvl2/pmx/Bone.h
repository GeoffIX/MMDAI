/**

 Copyright (c) 2010-2014  hkrn

 All rights reserved.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following
 conditions are met:

 - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.
 - Neither the name of the MMDAI project team nor the names of
   its contributors may be used to endorse or promote products
   derived from this software without specific prior written
   permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once
#ifndef VPVL2_PMX_BONE_H_
#define VPVL2_PMX_BONE_H_

#include "vpvl2/IBone.h"
#include "vpvl2/pmx/Model.h"
#include "vpvl2/pmx/Morph.h"

namespace vpvl2
{
namespace VPVL2_VERSION_NS
{
namespace pmx
{

/**
 * @file
 * @author hkrn
 *
 * @section DESCRIPTION
 *
 * Bone class represents a bone of a Polygon Model Extended object.
 */

class VPVL2_API Bone VPVL2_DECL_FINAL : public IBone, public IBone::IKConstraint
{
public:
    enum Flags {
        kHasDestinationOrigin      = 0x1,
        kRotatetable               = 0x2,
        kMovable                   = 0x4,
        kVisible                   = 0x8,
        kInteractive               = 0x10,
        kHasInverseKinematics      = 0x20,
        kHasInherentRotation      = 0x100,
        kHasInherentTranslation   = 0x200,
        kHasFixedAxis              = 0x400,
        kHasLocalAxes              = 0x800,
        kTransformAfterPhysics     = 0x1000,
        kTransformByExternalParent = 0x2000
    };

    /**
     * Constructor
     */
    Bone(Model *modelRef);
    ~Bone();

    static bool preparse(uint8 *&ptr, vsize &rest, Model::DataInfo &info);
    static bool loadBones(const Array<Bone *> &bones);
    static void sortBones(const Array<Bone *> &bones, Array<Bone *> &bpsBones, Array<Bone *> &apsBones);
    static void writeBones(const Array<Bone *> &bones, const Model::DataInfo &info, uint8 *&data);
    static vsize estimateTotalSize(const Array<Bone *> &bones, const Model::DataInfo &info);

    void read(const uint8 *data, const Model::DataInfo &info, vsize &size);
    void write(uint8 *&data, const Model::DataInfo &info) const;
    vsize estimateSize(const Model::DataInfo &info) const;
    void mergeMorph(const Morph::Bone *morph, const IMorph::WeightPrecision &weight);
    void getLocalTransform(Transform &output) const;
    void getLocalTransform(const Transform &worldTransform, Transform &output) const;
    void performTransform();
    void solveInverseKinematics();
    void updateLocalTransform();
    void reset();
    Vector3 offset() const;
    Transform worldTransform() const;
    Transform localTransform() const;
    void getEffectorBones(Array<IBone *> &value) const;

    void setLocalTranslation(const Vector3 &value);
    void setLocalOrientation(const Quaternion &value);
    Vector3 fixedAxis() const;
    void getLocalAxes(Matrix3x3 &value) const;
    void setLocalTransform(const Transform &value);
    void setSimulated(bool value);

    Label *internalParentLabelRef() const;
    IModel *parentModelRef() const;
    IBone *parentBoneRef() const;
    IBone *parentInherentBoneRef() const;
    IBone *destinationOriginBoneRef() const;
    const IString *name(IEncoding::LanguageType type) const;
    Quaternion localOrientation() const;
    Vector3 origin() const;
    Vector3 destinationOrigin() const;
    Vector3 localTranslation() const;
    Vector3 axis() const;
    Vector3 axisX() const;
    Vector3 axisZ() const;
    float32 constraintAngle() const;
    float32 inherentCoefficient() const;
    int index() const;
    int layerIndex() const;
    int externalIndex() const;

    bool isRotateable() const;
    bool isMovable() const;
    bool isVisible() const;
    bool isInteractive() const;
    bool hasInverseKinematics() const;
    bool isInherentOrientationEnabled() const;
    bool isInherentTranslationEnabled() const;
    bool hasFixedAxes() const;
    bool hasLocalAxes() const;
    bool isTransformedAfterPhysicsSimulation() const;
    bool isTransformedByExternalParent() const;
    bool isInverseKinematicsEnabled() const;

    void setInternalParentLabelRef(Label *value);
    void setParentBoneRef(IBone *value);
    void setParentInherentBoneRef(IBone *value);
    void setInherentCoefficient(float32 value);
    void setDestinationOriginBoneRef(IBone *value);
    void setName(const IString *value, IEncoding::LanguageType type);
    void setOrigin(const Vector3 &value);
    void setDestinationOrigin(const Vector3 &value);
    void setFixedAxis(const Vector3 &value);
    void setAxisX(const Vector3 &value);
    void setAxisZ(const Vector3 &value);
    void setIndex(int value);
    void setLayerIndex(int value);
    void setExternalIndex(int value);
    void setRotateable(bool value);
    void setMovable(bool value);
    void setVisible(bool value);
    void setInteractive(bool value);
    void setHasInverseKinematics(bool value);
    void setInherentOrientationEnable(bool value);
    void setInherentTranslationEnable(bool value);
    void setFixedAxisEnable(bool value);
    void setLocalAxesEnable(bool value);
    void setTransformAfterPhysicsEnable(bool value);
    void setTransformedByExternalParentEnable(bool value);
    void setInverseKinematicsEnable(bool value);

    IBone *rootBoneRef() const;
    void setRootBoneRef(IBone *value);
    IBone *effectorBoneRef() const;
    void setEffectorBoneRef(IBone *value);
    int numIterations() const;
    void setNumIterations(int value);
    float32 angleLimit() const;
    void setAngleLimit(float32 value);
    void getJointRefs(Array<IKJoint *> &value) const;

private:
    struct PrivateContext;
    PrivateContext *m_context;

    VPVL2_DISABLE_COPY_AND_ASSIGN(Bone)
};

} /* namespace pmx */
} /* namespace VPVL2_VERSION_NS */
} /* namespace vpvl2 */

#endif
