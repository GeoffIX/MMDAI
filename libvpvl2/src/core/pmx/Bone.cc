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

#include "vpvl2/vpvl2.h"
#include "vpvl2/internal/ModelHelper.h"

#include "vpvl2/pmx/Bone.h"
#include "vpvl2/pmx/Label.h"

#ifdef VPVL2_NEW_IK
#define VPVL2_IK_COND(a, b) a
#else
#define VPVL2_IK_COND(a, b) b
#endif

namespace
{

using namespace vpvl2::VPVL2_VERSION_NS;

#pragma pack(push, 1)

struct BoneUnit {
    float32 vector3[3];
};

struct IKUnit {
    int32 numIterations;
    float32 angleLimit;
    int32 numConstraints;
};

#pragma pack(pop)

using namespace vpvl2::VPVL2_VERSION_NS;
using namespace vpvl2::VPVL2_VERSION_NS::pmx;

struct DefaultIKJoint : vpvl2::IBone::IKJoint {
    static void clampAngle(const Scalar &min, const Scalar &max, const Scalar &result, Scalar &output) {
        if (btFuzzyZero(min) && btFuzzyZero(max)) {
            output = 0;
        }
        else if (result < min) {
            output = min;
        }
        else if (result > max) {
            output = max;
        }
    }
    static Scalar clampAngle2(const Scalar &value, const Scalar &lower, const Scalar &upper, bool ikt) {
        Scalar v = value;
        (void) ikt;
        if (v < lower) {
            const Scalar &tf = 2 * lower - v;
            v = (tf <= upper && ikt) ? tf : lower;
        }
        if (v > upper) {
            const Scalar &tf = 2 * upper - v;
            v = (tf >= lower && ikt) ? tf : upper;
        }
        return v;
    }
    static void setRotation(const Scalar &x, const Scalar &y, const Scalar &z,
                            const Vector3 &lowerLimit, const Vector3 &upperLimit, bool ikt,
                            Quaternion &rx, Quaternion &ry, Quaternion &rz) {
        const Scalar &x2 = clampAngle2(x, lowerLimit.x(), upperLimit.x(), ikt);
        const Scalar &y2 = clampAngle2(y, lowerLimit.y(), upperLimit.y(), ikt);
        const Scalar &z2 = clampAngle2(z, lowerLimit.z(), upperLimit.z(), ikt);
        rx.setRotation(kUnitX, x2);
        ry.setRotation(kUnitY, y2);
        rz.setRotation(kUnitZ, z2);
    }

    DefaultIKJoint(const Bone *parentBoneRef)
        : m_parentBoneRef(parentBoneRef),
          m_targetBoneRef(0),
          m_targetBoneIndex(-1),
          m_hasAngleLimit(false),
          m_lowerLimit(vpvl2::kZeroV3),
          m_upperLimit(vpvl2::kZeroV3)
    {
    }
    ~DefaultIKJoint() {
        m_targetBoneRef = 0;
        m_targetBoneIndex = -1;
        m_hasAngleLimit = false;
        m_lowerLimit.setZero();
        m_upperLimit.setZero();
    }

    IBone *targetBoneRef() const {
        return m_targetBoneRef;
    }
    void setTargetBoneRef(IBone *value) {
        if (value && value->parentModelRef()->type() == IModel::kPMXModel) {
            m_targetBoneRef = static_cast<pmx::Bone *>(value);
            m_targetBoneIndex = value->index();
        }
    }
    bool hasAngleLimit() const {
        return m_hasAngleLimit;
    }
    void setHasAngleLimit(bool value) {
        m_hasAngleLimit = value;
    }
    Vector3 lowerLimit() const {
        return m_lowerLimit;
    }
    void setLowerLimit(const Vector3 &value) {
        m_lowerLimit = value;
    }
    Vector3 upperLimit() const {
        return m_upperLimit;
    }
    void setUpperLimit(const Vector3 &value) {
        m_upperLimit = value;
    }

    bool calculateAxisAngle(const Vector3 &rootBonePosition, Vector3 &localAxis, Scalar &angle) const {
        const Vector3 &currentEffectorPosition = m_parentBoneRef->effectorBoneRef()->worldTransform().getOrigin();
        const Transform &jointBoneTransform = m_targetBoneRef->worldTransform();
        const Transform &inversedJointBoneTransform = jointBoneTransform.inverse();
        Vector3 localRootBonePosition = inversedJointBoneTransform * rootBonePosition;
        Vector3 localEffectorPosition = inversedJointBoneTransform * currentEffectorPosition;
        localRootBonePosition.normalize();
        localEffectorPosition.normalize();
        localAxis = localEffectorPosition.cross(localRootBonePosition).safeNormalize();
        const Scalar &dot = localRootBonePosition.dot(localEffectorPosition);
        if (!btFuzzyZero(dot)) {
            angle = btAcos(btClamped(dot, -1.0f, 1.0f));
            return true;
        }
        else {
            angle = 0;
            return false;
        }
    }
    void transformLocalAxis(bool performConstraint, Vector3 &localAxis) const {
#ifdef VPVL2_NEW_IK
        if (hasAngleLimit() && performConstraint) {
            const Matrix3x3 &matrix = m_targetBoneRef->worldTransform().getBasis();
            constrainLocalAxis(matrix, localAxis);
        }
        else {
            localAxis = m_targetBoneRef->localTransform() * localAxis;
        }
#else
        (void) performConstraint;
        (void) localAxis;
#endif
    }
    void constrainLocalAxis(const Matrix3x3 &matrix, Vector3 &localAxis) const {
        (void) matrix;
        if (btFuzzyZero(m_lowerLimit.y()) && btFuzzyZero(m_upperLimit.y())
                && btFuzzyZero(m_lowerLimit.z()) && btFuzzyZero(m_upperLimit.z())) {
            const Scalar &axisX = VPVL2_IK_COND(btSelect(localAxis.dot(matrix.getColumn(0)) >= 0, 1.0f, -1.0f), 1.0f);
            localAxis.setValue(axisX, 0.0, 0.0);
        }
        else if (btFuzzyZero(m_lowerLimit.x()) && btFuzzyZero(m_upperLimit.x())
                 && btFuzzyZero(m_lowerLimit.z()) && btFuzzyZero(m_upperLimit.z())) {
            const Scalar &axisY = VPVL2_IK_COND(btSelect(localAxis.dot(matrix.getColumn(1)) >= 0, 1.0f, -1.0f), 1.0f);
            localAxis.setValue(0.0, axisY, 0.0);
        }
        else if (btFuzzyZero(m_lowerLimit.x()) && btFuzzyZero(m_upperLimit.x())
                 && btFuzzyZero(m_lowerLimit.y()) && btFuzzyZero(m_upperLimit.y())) {
            const Scalar &axisZ = VPVL2_IK_COND(btSelect(localAxis.dot(matrix.getColumn(2)) >= 0, 1.0f, -1.0f), 1.0f);
            localAxis.setValue(0.0, 0.0, axisZ);
        }
    }
    void constrainRotation(Quaternion &jointRotation, bool performConstrain) const {
#ifndef VPVL2_NEW_IK
        (void) performConstrain;
        Scalar x1, y1, z1, x2, y2, z2, x3, y3, z3;
        Matrix3x3 matrix(jointRotation);
        matrix.getEulerZYX(z1, y1, x1);
        matrix.setRotation(m_targetBoneRef->localOrientation());
        matrix.getEulerZYX(z2, y2, x2);
        x3 = x1 + x2; y3 = y1 + y2; z3 = z1 + z2;
        clampAngle(m_lowerLimit.x(), m_upperLimit.x(), x3, x1);
        clampAngle(m_lowerLimit.y(), m_upperLimit.y(), y3, y1);
        clampAngle(m_lowerLimit.z(), m_upperLimit.z(), z3, z1);
        jointRotation.setEulerZYX(z1, y1, x1);
#else
        //
        // extraction order in 2.1 and 2.4 and 2.5 from
        // http://www.geometrictools.com/Documentation/EulerAngles.pdf
        //
        static const Scalar &kEulerAngleLimit = btRadians(90), &kEulerAngleLimit2 = btRadians(88);
        Scalar x, y, z;
        Quaternion rx, ry, rz, result;
        Matrix3x3 matrix(jointRotation);
        // ZXY
        if (m_lowerLimit.x() > -kEulerAngleLimit && m_upperLimit.x() < kEulerAngleLimit) {
            z = btClamped(btAsin(matrix[1][2]), -kEulerAngleLimit2, kEulerAngleLimit2);
            x = btAtan2(-matrix[1][0], matrix[1][1]);
            y = btAtan2(-matrix[0][2], matrix[2][2]);
            setRotation(x, y, z, m_lowerLimit, m_upperLimit, performConstrain, rx, ry, rz);
            result = ry * rx * rz;
        }
        // XYZ
        else if (m_lowerLimit.y() > -kEulerAngleLimit && m_upperLimit.y() < kEulerAngleLimit) {
            y = btClamped(btAsin(matrix[2][0]), -kEulerAngleLimit2, kEulerAngleLimit2);
            x = btAtan2(-matrix[2][1], matrix[2][2]);
            z = btAtan2(-matrix[1][0], matrix[0][0]);
            setRotation(x, y, z, m_lowerLimit, m_upperLimit, performConstrain, rx, ry, rz);
            result = rz * ry * rx;
        }
        // YZX
        else {
            z = btClamped(btAsin(matrix[0][1]), -kEulerAngleLimit2, kEulerAngleLimit2);
            y = btAtan2(-matrix[0][2], matrix[0][0]);
            x = btAtan2(-matrix[2][1], matrix[1][1]);
            setRotation(x, y, z, m_lowerLimit, m_upperLimit, performConstrain, rx, ry, rz);
            result = rx * rz * ry;
        }
        jointRotation = result.normalized();
#endif
    }

    const Bone *m_parentBoneRef;
    Bone *m_targetBoneRef;
    int m_targetBoneIndex;
    bool m_hasAngleLimit;
    Vector3 m_lowerLimit;
    Vector3 m_upperLimit;
};

struct BoneOrderPredication {
    inline bool operator()(const Bone *left, const Bone *right) const {
        if (left->isTransformedAfterPhysicsSimulation() == right->isTransformedAfterPhysicsSimulation()) {
            if (left->layerIndex() == right->layerIndex())
                return left->index() < right->index();
            return left->layerIndex() < right->layerIndex();
        }
        return right->isTransformedAfterPhysicsSimulation();
    }
};

}

namespace vpvl2
{
namespace VPVL2_VERSION_NS
{
namespace pmx
{

struct Bone::PrivateContext {
    PrivateContext(Model *modelRef)
        : parentModelRef(modelRef),
          parentLabelRef(0),
          parentBoneRef(0),
          effectorBoneRef(0),
          parentInherentBoneRef(0),
          destinationOriginBoneRef(0),
          namePtr(0),
          englishNamePtr(0),
          localOrientation(Quaternion::getIdentity()),
          localInherentOrientation(Quaternion::getIdentity()),
          localMorphOrientation(Quaternion::getIdentity()),
          jointOrientation(Quaternion::getIdentity()),
          worldTransform(Transform::getIdentity()),
          localTransform(Transform::getIdentity()),
          origin(kZeroV3),
          offsetFromParent(kZeroV3),
          localTranslation(kZeroV3),
          localInherentTranslation(kZeroV3),
          localMorphTranslation(kZeroV3),
          destinationOrigin(kZeroV3),
          fixedAxis(kZeroV3),
          axisX(kZeroV3),
          axisZ(kZeroV3),
          angleLimit(0.0),
          coefficient(1.0),
          index(-1),
          parentBoneIndex(-1),
          layerIndex(0),
          destinationOriginBoneIndex(-1),
          effectorBoneIndex(-1),
          numIterations(0),
          parentInherentBoneIndex(-1),
          globalID(0),
          flags(0),
          enableInverseKinematics(true)
    {
    }
    ~PrivateContext() {
        joints.releaseAll();
        internal::deleteObject(namePtr);
        internal::deleteObject(englishNamePtr);
        parentLabelRef = 0;
        parentModelRef = 0;
        parentBoneRef = 0;
        effectorBoneRef = 0;
        parentInherentBoneRef = 0;
        origin.setZero();
        offsetFromParent.setZero();
        localTranslation.setZero();
        localMorphTranslation.setZero();
        worldTransform.setIdentity();
        localTransform.setIdentity();
        destinationOrigin.setZero();
        fixedAxis.setZero();
        axisX.setZero();
        axisZ.setZero();
        coefficient = 0;
        index = -1;
        parentBoneIndex = -1;
        layerIndex = 0;
        destinationOriginBoneIndex = -1;
        parentInherentBoneIndex = -1;
        globalID = 0;
        flags = 0;
        enableInverseKinematics = false;
    }

    static void setPositionToIKUnit(const Vector3 &inputLower,
                                    const Vector3 &inputUpper,
                                    float *outputLower,
                                    float *outputUpper)
    {
#ifdef VPVL2_COORDINATE_OPENGL
        outputLower[0] = -inputUpper.x();
        outputLower[1] = -inputUpper.y();
        outputLower[2] = inputLower.z();
        outputUpper[0] = -inputLower.x();
        outputUpper[1] = -inputLower.y();
        outputUpper[2] = inputUpper.z();
#else
        outputLower[0] = -inputUpper.x();
        outputLower[1] = -inputUpper.y();
        outputLower[2] = inputLower.z();
        outputUpper[0] = -inputLower.x();
        outputUpper[1] = -inputLower.y();
        outputUpper[2] = inputUpper.z();
#endif
    }
    static void getPositionFromIKUnit(const float *inputLower,
                                      const float *inputUpper,
                                      Vector3 &outputLower,
                                      Vector3 &outputUpper)
    {
#ifdef VPVL2_COORDINATE_OPENGL
        outputLower.setValue(-inputUpper[0], -inputUpper[1], inputLower[2]);
        outputUpper.setValue(-inputLower[0], -inputLower[1], inputUpper[2]);
#else
        outputLower.setValue(inputLower[0], inputLower[1], inputLower[2]);
        outputUpper.setValue(inputUpper[0], inputUpper[1], inputUpper[2]);
#endif
    }

    void updateWorldTransform() {
        updateWorldTransform(localTranslation, localOrientation);
    }
    void updateWorldTransform(const Vector3 &translation, const Quaternion &orientation) {
        worldTransform.setRotation(orientation);
        worldTransform.setOrigin(offsetFromParent + translation);
        if (parentBoneRef) {
            worldTransform = parentBoneRef->worldTransform() * worldTransform;
        }
    }

    Model *parentModelRef;
    Label *parentLabelRef;
    PointerArray<DefaultIKJoint> joints;
    Bone *parentBoneRef;
    Bone *effectorBoneRef;
    Bone *parentInherentBoneRef;
    Bone *destinationOriginBoneRef;
    IString *namePtr;
    IString *englishNamePtr;
    Quaternion localOrientation;
    Quaternion localInherentOrientation;
    Quaternion localMorphOrientation;
    Quaternion jointOrientation;
    Transform worldTransform;
    Transform localTransform;
    Vector3 origin;
    Vector3 offsetFromParent;
    Vector3 localTranslation;
    Vector3 localInherentTranslation;
    Vector3 localMorphTranslation;
    Vector3 destinationOrigin;
    Vector3 fixedAxis;
    Vector3 axisX;
    Vector3 axisZ;
    float32 angleLimit;
    float32 coefficient;
    int index;
    int parentBoneIndex;
    int layerIndex;
    int destinationOriginBoneIndex;
    int effectorBoneIndex;
    int numIterations;
    int parentInherentBoneIndex;
    int globalID;
    uint16 flags;
    bool enableInverseKinematics;
};

Bone::Bone(Model *modelRef)
    : m_context(new PrivateContext(modelRef))
{
}

Bone::~Bone()
{
    if (Label *parentLabelRef = m_context->parentLabelRef) {
        parentLabelRef->removeBoneRef(this);
    }
    internal::deleteObject(m_context);
}

bool Bone::preparse(uint8 *&ptr, vsize &rest, Model::DataInfo &info)
{
    int32 nbones = 0, size = 0, boneIndexSize = int32(info.boneIndexSize);
    if (!internal::getTyped<int32>(ptr, rest, nbones)) {
        VPVL2_LOG(WARNING, "Invalid size of PMX bones detected: size=" << nbones << " rest=" << rest);
        return false;
    }
    info.bonesPtr = ptr;
    /* BoneUnit + boneIndexSize + hierarcy + flags */
    vsize baseSize = sizeof(BoneUnit) + boneIndexSize + sizeof(int32) + sizeof(uint16);
    for (int32 i = 0; i < nbones; i++) {
        uint8 *namePtr;
        /* name in Japanese */
        if (!internal::getText(ptr, rest, namePtr, size)) {
            VPVL2_LOG(WARNING, "Invalid size of PMX bone name in Japanese detected: index=" << i << " size=" << size << " rest=" << rest);
            return false;
        }
        /* name in English */
        if (!internal::getText(ptr, rest, namePtr, size)) {
            VPVL2_LOG(WARNING, "Invalid size of PMX bone name in English detected: index=" << i << " size=" << size << " rest=" << rest);
            return false;
        }
        if (!internal::validateSize(ptr, baseSize, rest)) {
            VPVL2_LOG(WARNING, "Invalid size of PMX bone base structure detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
            return false;
        }
        uint16 flags = *reinterpret_cast<uint16 *>(ptr - 2);
        /* bone has destination relative or absolute */
        BoneUnit p;
        if (internal::hasFlagBits(flags, kHasDestinationOrigin)) {
            if (!internal::validateSize(ptr, boneIndexSize, rest)) {
                VPVL2_LOG(WARNING, "Invalid size of PMX destination bone index detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
                return false;
            }
        }
        else {
            p = *reinterpret_cast<const BoneUnit *>(ptr);
            if (!internal::validateSize(ptr, sizeof(BoneUnit), rest)) {
                VPVL2_LOG(WARNING, "Invalid size of PMX destination bone unit detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
                return false;
            }
        }
        /* bone has additional bias */
        if ((internal::hasFlagBits(flags, kHasInherentRotation) ||
             internal::hasFlagBits(flags, kHasInherentTranslation)) &&
                !internal::validateSize(ptr, boneIndexSize + sizeof(float), rest)) {
            VPVL2_LOG(WARNING, "Invalid size of PMX inherence bone index detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
            return false;
        }
        /* axis of bone is fixed */
        if (internal::hasFlagBits(flags, kHasFixedAxis) && !internal::validateSize(ptr, sizeof(BoneUnit), rest)) {
            VPVL2_LOG(WARNING, "Invalid size of PMX fixed bone axis detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
            return false;
        }
        /* axis of bone is local */
        if (internal::hasFlagBits(flags, kHasLocalAxes) && !internal::validateSize(ptr, sizeof(BoneUnit) * 2, rest)) {
            VPVL2_LOG(WARNING, "Invalid size of PMX local bone axis detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
            return false;
        }
        /* bone is transformed after external parent bone transformation */
        if (internal::hasFlagBits(flags, kTransformByExternalParent) && !internal::validateSize(ptr, sizeof(int), rest)) {
            VPVL2_LOG(WARNING, "Invalid size of PMX external parent index detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
            return false;
        }
        /* bone is IK */
        if (internal::hasFlagBits(flags, kHasInverseKinematics)) {
            /* boneIndex + IK loop count + IK constraint radian per once + IK link count */
            vsize extraSize = boneIndexSize + sizeof(IKUnit);
            const IKUnit &unit = *reinterpret_cast<const IKUnit *>(ptr + boneIndexSize);
            if (!internal::validateSize(ptr, extraSize, rest)) {
                VPVL2_LOG(WARNING, "Invalid size of PMX IK unit detected: index=" << i << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
                return false;
            }
            int nconstraints = unit.numConstraints;
            for (int j = 0; j < nconstraints; j++) {
                if (!internal::validateSize(ptr, boneIndexSize, rest)) {
                    VPVL2_LOG(WARNING, "Invalid size of PMX IK joint bone index detected: index=" << i << " joint=" << j << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
                    return false;
                }
                uint8 hasAngleLimit;
                if (!internal::getTyped<uint8>(ptr, rest, hasAngleLimit)) {
                    VPVL2_LOG(WARNING, "Invalid size of PMX IK constraint detected: index=" << i << " joint=" << j << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
                    return false;
                }
                if (hasAngleLimit == 1 && !internal::validateSize(ptr, sizeof(BoneUnit) * 2, rest)) {
                    VPVL2_LOG(WARNING, "Invalid size of PMX IK angle constraint detected: index=" << i << " joint=" << j << " ptr=" << static_cast<const void *>(ptr) << " rest=" << rest);
                    return false;
                }
            }
        }
    }
    info.bonesCount = nbones;
    return true;
}

bool Bone::loadBones(const Array<Bone *> &bones)
{
    const int nbones = bones.count();
    for (int i = 0; i < nbones; i++) {
        Bone *boneRef = bones[i];
        const int parentBoneIndex = boneRef->m_context->parentBoneIndex;
        if (parentBoneIndex >= 0) {
            if (parentBoneIndex >= nbones) {
                VPVL2_LOG(WARNING, "Invalid PMX parentBoneIndex specified: index=" << i << " bone=" << parentBoneIndex);
                return false;
            }
            else {
                Bone *parentBoneRef = bones[parentBoneIndex];
                boneRef->m_context->offsetFromParent -= parentBoneRef->m_context->origin;
                boneRef->m_context->parentBoneRef = parentBoneRef;
            }
        }
        const int destinationOriginBoneIndex = boneRef->m_context->destinationOriginBoneIndex;
        if (destinationOriginBoneIndex >= 0) {
            if (destinationOriginBoneIndex >= nbones) {
                VPVL2_LOG(WARNING, "Invalid PMX destinationOriginBoneIndex specified: index=" << i << " bone=" << destinationOriginBoneIndex);
                return false;
            }
            else {
                boneRef->m_context->destinationOriginBoneRef = bones[destinationOriginBoneIndex];
            }
        }
        const int targetBoneIndex = boneRef->m_context->effectorBoneIndex;
        if (targetBoneIndex >= 0) {
            if (targetBoneIndex >= nbones) {
                VPVL2_LOG(WARNING, "Invalid PMX targetBoneIndex specified: index=" << i << " bone=" << targetBoneIndex);
                return false;
            }
            else {
                boneRef->m_context->effectorBoneRef = bones[targetBoneIndex];
            }
        }
        const int parentInherentBoneIndex = boneRef->m_context->parentInherentBoneIndex;
        if (parentInherentBoneIndex >= 0) {
            if (parentInherentBoneIndex >= nbones) {
                VPVL2_LOG(WARNING, "Invalid PMX parentInherentBoneIndex specified: index=" << i << " bone=" << parentInherentBoneIndex);
                return false;
            }
            else {
                boneRef->m_context->parentInherentBoneRef = bones[parentInherentBoneIndex];
            }
        }
        if (boneRef->hasInverseKinematics()) {
            const Array<DefaultIKJoint *> &constraints = boneRef->m_context->joints;
            const int nconstraints = constraints.count();
            for (int j = 0; j < nconstraints; j++) {
                DefaultIKJoint *constraint = constraints[j];
                const int jointBoneIndex = constraint->m_targetBoneIndex;
                if (jointBoneIndex >= 0) {
                    if (jointBoneIndex >= nbones) {
                        VPVL2_LOG(WARNING, "Invalid PMX jointBoneIndex specified: index=" << i << " joint=" << j << " bone=" << jointBoneIndex);
                        return false;
                    }
                    else {
                        constraint->m_targetBoneRef = bones[jointBoneIndex];
                    }
                }
            }
        }
        boneRef->setIndex(i);
    }
    return true;
}

void Bone::sortBones(const Array<Bone *> &bones, Array<Bone *> &bpsBones, Array<Bone *> &apsBones)
{
    Array<Bone *> orderedBonesRefs;
    orderedBonesRefs.copy(bones);
    orderedBonesRefs.sort(BoneOrderPredication());
    bpsBones.clear();
    apsBones.clear();
    const int nbones = bones.count();
    for (int i = 0; i < nbones; i++) {
        Bone *bone = orderedBonesRefs[i];
        if (bone->isTransformedAfterPhysicsSimulation()) {
            apsBones.append(bone);
        }
        else {
            bpsBones.append(bone);
        }
    }
}

void Bone::writeBones(const Array<Bone *> &bones, const Model::DataInfo &info, uint8 *&data)
{
    const int nbones = bones.count();
    internal::writeBytes(&nbones, sizeof(nbones), data);
    for (int i = 0; i < nbones; i++) {
        const Bone *bone = bones[i];
        bone->write(data, info);
    }
}

vsize Bone::estimateTotalSize(const Array<Bone *> &bones, const Model::DataInfo &info)
{
    const int nbones = bones.count();
    vsize size = 0;
    size += sizeof(nbones);
    for (int i = 0; i < nbones; i++) {
        Bone *bone = bones[i];
        size += bone->estimateSize(info);
    }
    return size;
}

void Bone::read(const uint8 *data, const Model::DataInfo &info, vsize &size)
{
    uint8 *namePtr = 0, *ptr = const_cast<uint8 *>(data), *start = ptr;
    vsize rest = SIZE_MAX, boneIndexSize = info.boneIndexSize;
    int32 nNameSize;
    IEncoding *encoding = info.encoding;
    internal::getText(ptr, rest, namePtr, nNameSize);
    internal::setStringDirect(encoding->toString(namePtr, nNameSize, info.codec), m_context->namePtr);
    VPVL2_VLOG(3, "PMXBone: name=" << internal::cstr(m_context->namePtr, "(null)"));
    internal::getText(ptr, rest, namePtr, nNameSize);
    internal::setStringDirect(encoding->toString(namePtr, nNameSize, info.codec), m_context->englishNamePtr);
    VPVL2_VLOG(3, "PMXBone: englishName=" << internal::cstr(m_context->englishNamePtr, "(null)"));
    const BoneUnit &unit = *reinterpret_cast<const BoneUnit *>(ptr);
    internal::setPosition(unit.vector3, m_context->origin);
    VPVL2_VLOG(3, "PMXBone: origin=" << m_context->origin.x() << "," << m_context->origin.y() << "," << m_context->origin.z());
    m_context->offsetFromParent = m_context->origin;
    m_context->worldTransform.setOrigin(m_context->origin);
    ptr += sizeof(unit);
    m_context->parentBoneIndex = internal::readSignedIndex(ptr, boneIndexSize);
    VPVL2_VLOG(3, "PMXBone: parentBoneIndex=" << m_context->parentBoneIndex);
    m_context->layerIndex = *reinterpret_cast<int *>(ptr);
    ptr += sizeof(m_context->layerIndex);
    VPVL2_VLOG(3, "PMXBone: layerIndex=" << m_context->origin.x() << "," << m_context->origin.y() << "," << m_context->origin.z());
    uint16 flags = m_context->flags = *reinterpret_cast<uint16 *>(ptr);
    ptr += sizeof(m_context->flags);
    /* bone has destination */
    if (internal::hasFlagBits(flags, kHasDestinationOrigin)) {
        m_context->destinationOriginBoneIndex = internal::readSignedIndex(ptr, boneIndexSize);
        VPVL2_VLOG(3, "PMXBone: destinationOriginBoneIndex=" << m_context->destinationOriginBoneIndex);
    }
    else {
        BoneUnit offset;
        internal::getData(ptr, offset);
        internal::setPosition(offset.vector3, m_context->destinationOrigin);
        ptr += sizeof(offset);
        VPVL2_VLOG(3, "PMXBone: destinationOrigin=" << m_context->destinationOrigin.x()
                   << "," << m_context->destinationOrigin.y() << "," << m_context->destinationOrigin.z());
    }
    /* bone has additional bias */
    if (internal::hasFlagBits(flags, kHasInherentRotation) || internal::hasFlagBits(flags, kHasInherentTranslation)) {
        m_context->parentInherentBoneIndex = internal::readSignedIndex(ptr, boneIndexSize);
        internal::getData(ptr, m_context->coefficient);
        ptr += sizeof(m_context->coefficient);
        VPVL2_VLOG(3, "PMXBone: parentInherentBoneIndex=" << m_context->parentInherentBoneIndex << " weight=" << m_context->coefficient);
    }
    /* axis of bone is fixed */
    if (internal::hasFlagBits(flags, kHasFixedAxis)) {
        BoneUnit axis;
        internal::getData(ptr, axis);
        internal::setPosition(axis.vector3, m_context->fixedAxis);
        ptr += sizeof(axis);
        VPVL2_VLOG(3, "PMXBone: fixedAxis=" << m_context->fixedAxis.x() << "," << m_context->fixedAxis.y() << "," << m_context->fixedAxis.z());
    }
    /* axis of bone is local */
    if (internal::hasFlagBits(flags, kHasLocalAxes)) {
        BoneUnit axisX, axisZ;
        internal::getData(ptr, axisX);
        internal::setPosition(axisX.vector3, m_context->axisX);
        ptr += sizeof(axisX);
        VPVL2_VLOG(3, "PMXBone: localAxisX=" << m_context->axisX.x() << "," << m_context->axisX.y() << "," << m_context->axisX.z());
        internal::getData(ptr, axisZ);
        internal::setPosition(axisZ.vector3, m_context->axisZ);
        ptr += sizeof(axisZ);
        VPVL2_VLOG(3, "PMXBone: localAxisZ=" << m_context->axisZ.x() << "," << m_context->axisZ.y() << "," << m_context->axisZ.z());
    }
    /* bone is transformed after external parent bone transformation */
    if (internal::hasFlagBits(flags, kTransformByExternalParent)) {
        m_context->globalID = *reinterpret_cast<int *>(ptr);
        ptr += sizeof(m_context->globalID);
        VPVL2_VLOG(3, "PMXBone: externalBoneIndex=" << m_context->globalID);
    }
    /* bone is IK */
    if (internal::hasFlagBits(flags, kHasInverseKinematics)) {
        /* boneIndex + IK loop count + IK constraint radian per once + IK link count */
        m_context->effectorBoneIndex = internal::readSignedIndex(ptr, boneIndexSize);
        IKUnit iu;
        internal::getData(ptr, iu);
        m_context->numIterations = iu.numIterations;
        m_context->angleLimit = iu.angleLimit;
        VPVL2_VLOG(3, "PMXBone: targetBoneIndex=" << m_context->effectorBoneIndex << " nloop=" << m_context->numIterations << " angle=" << m_context->angleLimit);
        int nlinks = iu.numConstraints;
        ptr += sizeof(iu);
        for (int i = 0; i < nlinks; i++) {
            DefaultIKJoint *constraint = m_context->joints.append(new DefaultIKJoint(this));
            constraint->m_targetBoneIndex = internal::readSignedIndex(ptr, boneIndexSize);
            constraint->m_hasAngleLimit = *reinterpret_cast<uint8 *>(ptr) == 1;
            VPVL2_VLOG(3, "PMXBone: boneIndex=" << constraint->m_targetBoneIndex << " hasRotationConstraint" << constraint->m_hasAngleLimit);
            ptr += sizeof(constraint->m_hasAngleLimit);
            if (constraint->m_hasAngleLimit) {
                BoneUnit lower, upper;
                internal::getData(ptr, lower);
                ptr += sizeof(lower);
                internal::getData(ptr, upper);
                ptr += sizeof(upper);
                PrivateContext::getPositionFromIKUnit(&lower.vector3[0], &upper.vector3[0], constraint->m_lowerLimit, constraint->m_upperLimit);
                VPVL2_VLOG(3, "PMXBone: lowerLimit=" << constraint->m_lowerLimit.x() << "," << constraint->m_lowerLimit.y() << "," << constraint->m_lowerLimit.z());
                VPVL2_VLOG(3, "PMXBone: upperLimit=" << constraint->m_upperLimit.x() << "," << constraint->m_upperLimit.y() << "," << constraint->m_upperLimit.z());
            }
        }
    }
    size = ptr - start;
}

void Bone::write(uint8 *&data, const Model::DataInfo &info) const
{
    vsize boneIndexSize = info.boneIndexSize;
    BoneUnit bu;
    internal::writeString(m_context->namePtr, info.encoding, info.codec, data);
    internal::writeString(m_context->englishNamePtr, info.encoding, info.codec, data);
    internal::getPosition(m_context->origin, &bu.vector3[0]);
    internal::writeBytes(&bu, sizeof(bu), data);
    internal::writeSignedIndex(m_context->parentBoneIndex, boneIndexSize, data);
    internal::writeBytes(&m_context->layerIndex, sizeof(m_context->layerIndex), data);
    internal::writeBytes(&m_context->flags, sizeof(m_context->flags), data);
    if (internal::hasFlagBits(m_context->flags, kHasDestinationOrigin)) {
        internal::writeSignedIndex(m_context->destinationOriginBoneIndex, boneIndexSize, data);
    }
    else {
        internal::getPosition(m_context->destinationOrigin, &bu.vector3[0]);
        internal::writeBytes(&bu, sizeof(bu), data);
    }
    if (isInherentOrientationEnabled() || isInherentTranslationEnabled()) {
        internal::writeSignedIndex(m_context->parentInherentBoneIndex, boneIndexSize, data);
        internal::writeBytes(&m_context->coefficient, sizeof(m_context->coefficient), data);
    }
    if (hasFixedAxes()) {
        internal::getPosition(m_context->fixedAxis, &bu.vector3[0]);
        internal::writeBytes(&bu, sizeof(bu), data);
    }
    if (hasLocalAxes()) {
        internal::getPosition(m_context->axisX, &bu.vector3[0]);
        internal::writeBytes(&bu, sizeof(bu), data);
        internal::getPosition(m_context->axisZ, &bu.vector3[0]);
        internal::writeBytes(&bu, sizeof(bu), data);
    }
    if (isTransformedByExternalParent()) {
        internal::writeBytes(&m_context->globalID, sizeof(m_context->globalID), data);
    }
    if (hasInverseKinematics()) {
        internal::writeSignedIndex(m_context->effectorBoneIndex, boneIndexSize, data);
        IKUnit iku;
        iku.angleLimit = m_context->angleLimit;
        iku.numIterations = m_context->numIterations;
        const int nconstarints = iku.numConstraints = m_context->joints.count();
        internal::writeBytes(&iku, sizeof(iku), data);
        BoneUnit lower, upper;
        for (int i = 0; i < nconstarints; i++) {
            DefaultIKJoint *constraint = m_context->joints[i];
            internal::writeSignedIndex(constraint->m_targetBoneIndex, boneIndexSize, data);
            uint8 hasAngleLimit = constraint->m_hasAngleLimit ? 1 : 0;
            internal::writeBytes(&hasAngleLimit, sizeof(hasAngleLimit), data);
            if (hasAngleLimit) {
                PrivateContext::setPositionToIKUnit(constraint->m_lowerLimit, constraint->m_upperLimit, &lower.vector3[0], &upper.vector3[0]);
                internal::writeBytes(&lower.vector3, sizeof(lower.vector3), data);
                internal::writeBytes(&upper.vector3, sizeof(upper.vector3), data);
            }
        }
    }
}

vsize Bone::estimateSize(const Model::DataInfo &info) const
{
    vsize size = 0, boneIndexSize = info.boneIndexSize;
    size += internal::estimateSize(m_context->namePtr, info.encoding, info.codec);
    size += internal::estimateSize(m_context->englishNamePtr, info.encoding, info.codec);
    size += sizeof(BoneUnit);
    size += boneIndexSize;
    size += sizeof(m_context->layerIndex);
    size += sizeof(m_context->flags);
    size += (internal::hasFlagBits(m_context->flags, kHasDestinationOrigin)) ? boneIndexSize : sizeof(BoneUnit);
    if (isInherentOrientationEnabled() || isInherentTranslationEnabled()) {
        size += boneIndexSize;
        size += sizeof(m_context->coefficient);
    }
    if (hasFixedAxes()) {
        size += sizeof(BoneUnit);
    }
    if (hasLocalAxes()) {
        size += sizeof(BoneUnit) * 2;
    }
    if (isTransformedByExternalParent()) {
        size += sizeof(m_context->globalID);
    }
    if (hasInverseKinematics()) {
        size += boneIndexSize;
        size += sizeof(IKUnit);
        const Array<DefaultIKJoint *> &constraints = m_context->joints;
        int nconstraints = constraints.count();
        for (int i = 0; i < nconstraints; i++) {
            size += boneIndexSize;
            size += sizeof(uint8);
            if (constraints[i]->m_hasAngleLimit) {
                size += sizeof(BoneUnit) * 2;
            }
        }
    }
    return size;
}

void Bone::mergeMorph(const Morph::Bone *morph, const IMorph::WeightPrecision &weight)
{
    const Scalar &w = Scalar(weight);
    m_context->localMorphTranslation += morph->position * w;
    m_context->localMorphOrientation *= Quaternion::getIdentity().slerp(morph->rotation, w);
}

void Bone::getLocalTransform(Transform &output) const
{
    getLocalTransform(m_context->worldTransform, output);
}

void Bone::getLocalTransform(const Transform &worldTransform, Transform &output) const
{
    output = worldTransform * Transform(Matrix3x3::getIdentity(), -m_context->origin);
}

void Bone::performTransform()
{
    Quaternion orientation(Quaternion::getIdentity());
    if (isInherentOrientationEnabled()) {
        Bone *parentBoneRef = m_context->parentInherentBoneRef;
        if (parentBoneRef) {
            if (parentBoneRef->isInherentOrientationEnabled()) {
                orientation *= parentBoneRef->m_context->localInherentOrientation;
            }
            else {
                orientation *= parentBoneRef->localOrientation() * parentBoneRef->m_context->localMorphOrientation;
            }
        }
        if (!btFuzzyZero(m_context->coefficient - 1.0f)) {
            orientation = Quaternion::getIdentity().slerp(orientation, m_context->coefficient);
        }
        if (parentBoneRef && parentBoneRef->hasInverseKinematics()) {
            orientation *= parentBoneRef->m_context->jointOrientation;
        }
        m_context->localInherentOrientation = orientation * m_context->localOrientation * m_context->localMorphOrientation;
        m_context->localInherentOrientation.normalize();
    }
    orientation *= m_context->localOrientation * m_context->localMorphOrientation * m_context->jointOrientation;
    orientation.normalize();
    Vector3 translation(kZeroV3);
    if (isInherentTranslationEnabled()) {
        Bone *parentBone = m_context->parentInherentBoneRef;
        if (parentBone) {
            if (parentBone->isInherentTranslationEnabled()) {
                translation += parentBone->m_context->localInherentTranslation;
            }
            else {
                translation += parentBone->localTranslation() + parentBone->m_context->localMorphTranslation;
            }
        }
        if (!btFuzzyZero(m_context->coefficient - 1.0f)) {
            translation *= m_context->coefficient;
        }
        m_context->localInherentTranslation = translation;
    }
    translation += m_context->localTranslation + m_context->localMorphTranslation;
    m_context->updateWorldTransform(translation, orientation);
}

void Bone::solveInverseKinematics()
{
    if (!hasInverseKinematics() || !m_context->enableInverseKinematics) {
        return;
    }
    const Array<DefaultIKJoint *> &constraints = m_context->joints;
    const Vector3 &rootBonePosition = m_context->worldTransform.getOrigin();
    const int nconstraints = constraints.count();
    const int numIterations = m_context->numIterations;
    const int numHalfOfIteration = numIterations / 2;
    Bone *effectorBoneRef = m_context->effectorBoneRef;
    const Quaternion originalTargetRotation = effectorBoneRef->localOrientation();
    Quaternion jointRotation(Quaternion::getIdentity()), newJointLocalRotation;
    Vector3 localAxis(kZeroV3);
    Scalar angle = 0;
    for (int i = 0; i < numIterations; i++) {
        const bool performConstraint = i < numHalfOfIteration;
        for (int j = 0; j < nconstraints; j++) {
            const DefaultIKJoint *joint = constraints[j];
            if (!joint->calculateAxisAngle(rootBonePosition, localAxis, angle)) {
                break;
            }
            joint->transformLocalAxis(performConstraint, localAxis);
            const Scalar &angleLimit = m_context->angleLimit * (j + 1) * 2;
            jointRotation.setRotation(localAxis, btClamped(angle, -angleLimit, angleLimit));
            Bone *jointBoneRef = joint->m_targetBoneRef;
            if (joint->hasAngleLimit() && performConstraint) {
                if (VPVL2_IK_COND(performConstraint, i == 0)) {
                    joint->constrainLocalAxis(Matrix3x3(jointRotation), localAxis);
                    jointRotation.setRotation(localAxis, angle);
                }
                else {
                    joint->constrainRotation(jointRotation, performConstraint);
                }
                newJointLocalRotation = jointRotation * jointBoneRef->localOrientation();
            }
            else if (i == 0) {
                newJointLocalRotation = jointRotation * jointBoneRef->localOrientation();
            }
            else {
                newJointLocalRotation = jointBoneRef->localOrientation() * jointRotation;
            }
            jointBoneRef->setLocalOrientation(newJointLocalRotation);
            jointBoneRef->m_context->jointOrientation = jointRotation;
            for (int k = j; k >= 0; k--) {
                DefaultIKJoint *joint = constraints[k];
                jointBoneRef = joint->m_targetBoneRef;
                jointBoneRef->m_context->updateWorldTransform();
            }
            effectorBoneRef->m_context->updateWorldTransform();
        }
    }
    effectorBoneRef->setLocalOrientation(originalTargetRotation);
}

void Bone::updateLocalTransform()
{
    getLocalTransform(m_context->localTransform);
}

void Bone::reset()
{
    m_context->localMorphTranslation.setZero();
    m_context->localMorphOrientation = Quaternion::getIdentity();
    m_context->jointOrientation = Quaternion::getIdentity();
}

Vector3 Bone::offset() const
{
    return m_context->offsetFromParent;
}

Transform Bone::worldTransform() const
{
    return m_context->worldTransform;
}

Transform Bone::localTransform() const
{
    return m_context->localTransform;
}

void Bone::getEffectorBones(Array<IBone *> &value) const
{
    const Array<DefaultIKJoint *> &constraints = m_context->joints;
    const int nlinks = constraints.count();
    for (int i = 0; i < nlinks; i++) {
        DefaultIKJoint *constraint = constraints[i];
        IBone *bone = constraint->m_targetBoneRef;
        value.append(bone);
    }
}

void Bone::setLocalTranslation(const Vector3 &value)
{
    m_context->localTranslation = value;
}

void Bone::setLocalOrientation(const Quaternion &value)
{
    m_context->localOrientation = value;
}

Label *Bone::internalParentLabelRef() const
{
    return m_context->parentLabelRef;
}

IModel *Bone::parentModelRef() const
{
    return m_context->parentModelRef;
}

IBone *Bone::parentBoneRef() const
{
    return m_context->parentBoneRef;
}

IBone *Bone::parentInherentBoneRef() const
{
    return m_context->parentInherentBoneRef;
}

IBone *Bone::destinationOriginBoneRef() const
{
    return m_context->destinationOriginBoneRef;
}

const IString *Bone::name(IEncoding::LanguageType type) const
{
    switch (type) {
    case IEncoding::kDefaultLanguage:
    case IEncoding::kJapanese:
        return m_context->namePtr;
    case IEncoding::kEnglish:
        return m_context->englishNamePtr;
    default:
        return 0;
    }
}

Quaternion Bone::localOrientation() const
{
    return m_context->localOrientation;
}

Vector3 Bone::origin() const
{
    return m_context->origin;
}

Vector3 Bone::destinationOrigin() const
{
    if (const IBone *boneRef = m_context->destinationOriginBoneRef) {
        return boneRef->worldTransform().getOrigin();
    }
    else {
        return m_context->worldTransform.getOrigin() + m_context->worldTransform.getBasis() * m_context->destinationOrigin;
    }
}

Vector3 Bone::localTranslation() const
{
    return m_context->localTranslation;
}

Vector3 Bone::axis() const
{
    return m_context->fixedAxis;
}

Vector3 Bone::axisX() const
{
    return m_context->axisX;
}

Vector3 Bone::axisZ() const
{
    return m_context->axisZ;
}

float32 Bone::constraintAngle() const
{
    return m_context->angleLimit;
}

float32 Bone::inherentCoefficient() const
{
    return m_context->coefficient;
}

int Bone::index() const
{
    return m_context->index;
}

int Bone::layerIndex() const
{
    return m_context->layerIndex;
}

int Bone::externalIndex() const
{
    return m_context->globalID;
}

Vector3 Bone::fixedAxis() const
{
    return m_context->fixedAxis;
}

void Bone::getLocalAxes(Matrix3x3 &value) const
{
    if (hasLocalAxes()) {
        const Vector3 &axisY = m_context->axisZ.cross(m_context->axisX);
        const Vector3 &axisZ = m_context->axisX.cross(axisY);
        value[0] = m_context->axisX;
        value[1] = axisY;
        value[2] = axisZ;
    }
    else {
        value.setIdentity();
    }
}

bool Bone::isRotateable() const
{
    return internal::hasFlagBits(m_context->flags, kRotatetable);
}

bool Bone::isMovable() const
{
    return internal::hasFlagBits(m_context->flags, kMovable);
}

bool Bone::isVisible() const
{
    return internal::hasFlagBits(m_context->flags, kVisible);
}

bool Bone::isInteractive() const
{
    return internal::hasFlagBits(m_context->flags, kInteractive);
}

bool Bone::hasInverseKinematics() const
{
    return internal::hasFlagBits(m_context->flags, kHasInverseKinematics);
}

bool Bone::isInherentOrientationEnabled() const
{
    return internal::hasFlagBits(m_context->flags, kHasInherentRotation);
}

bool Bone::isInherentTranslationEnabled() const
{
    return internal::hasFlagBits(m_context->flags, kHasInherentTranslation);
}

bool Bone::hasFixedAxes() const
{
    return internal::hasFlagBits(m_context->flags, kHasFixedAxis);
}

bool Bone::hasLocalAxes() const
{
    return internal::hasFlagBits(m_context->flags, kHasLocalAxes);
}

bool Bone::isTransformedAfterPhysicsSimulation() const
{
    return internal::hasFlagBits(m_context->flags, kTransformAfterPhysics);
}

bool Bone::isTransformedByExternalParent() const
{
    return internal::hasFlagBits(m_context->flags, kTransformByExternalParent);
}

bool Bone::isInverseKinematicsEnabled() const
{
    return hasInverseKinematics() && m_context->enableInverseKinematics;
}

void Bone::setLocalTransform(const Transform &value)
{
    m_context->localTransform = value;
}

void Bone::setInternalParentLabelRef(Label *value)
{
    m_context->parentLabelRef = value;
}

void Bone::setParentBoneRef(IBone *value)
{
    if (!value || (value && value->parentModelRef() == m_context->parentModelRef)) {
        m_context->parentBoneRef = static_cast<Bone *>(value);
        m_context->parentBoneIndex = value ? value->index() : -1;
    }
}

void Bone::setParentInherentBoneRef(IBone *value)
{
    if (!value || (value && value->parentModelRef() == m_context->parentModelRef)) {
        m_context->parentInherentBoneRef = static_cast<Bone *>(value);
        m_context->parentInherentBoneIndex = value ? value->index() : -1;
    }
}

void Bone::setInherentCoefficient(float32 value)
{
    if (!btFuzzyZero(m_context->coefficient - value)) {
        m_context->coefficient = value;
    }
}

void Bone::setDestinationOriginBoneRef(IBone *value)
{
    if (!value || (value && value->parentModelRef() == m_context->parentModelRef)) {
        m_context->destinationOriginBoneRef = static_cast<Bone *>(value);
        m_context->destinationOriginBoneIndex = value ? value->index() : -1;
        internal::toggleFlag(kHasDestinationOrigin, value ? true : false, m_context->flags);
    }
}

void Bone::setName(const IString *value, IEncoding::LanguageType type)
{
    m_context->parentModelRef->removeBoneHash(this);
    internal::ModelHelper::setName(value, m_context->namePtr, m_context->englishNamePtr, type);
    m_context->parentModelRef->addBoneHash(this);
}

void Bone::setOrigin(const Vector3 &value)
{
    m_context->origin = value;
}

void Bone::setDestinationOrigin(const Vector3 &value)
{
    m_context->destinationOrigin = value;
    internal::toggleFlag(kHasDestinationOrigin, false, m_context->flags);
}

void Bone::setFixedAxis(const Vector3 &value)
{
    m_context->fixedAxis = value;
}

void Bone::setAxisX(const Vector3 &value)
{
    m_context->axisX = value;
}

void Bone::setAxisZ(const Vector3 &value)
{
    m_context->axisZ = value;
}

void Bone::setIndex(int value)
{
    m_context->index = value;
}

void Bone::setLayerIndex(int value)
{
    m_context->layerIndex = value;
}

void Bone::setExternalIndex(int value)
{
    m_context->globalID = value;
}

void Bone::setRotateable(bool value)
{
    internal::toggleFlag(kRotatetable, value, m_context->flags);
}

void Bone::setMovable(bool value)
{
    internal::toggleFlag(kMovable, value, m_context->flags);
}

void Bone::setVisible(bool value)
{
    internal::toggleFlag(kVisible, value, m_context->flags);
}

void Bone::setInteractive(bool value)
{
    internal::toggleFlag(kInteractive, value, m_context->flags);
}

void Bone::setHasInverseKinematics(bool value)
{
    internal::toggleFlag(kHasInverseKinematics, value, m_context->flags);
}

void Bone::setInherentOrientationEnable(bool value)
{
    internal::toggleFlag(kHasInherentTranslation, value, m_context->flags);
}

void Bone::setInherentTranslationEnable(bool value)
{
    internal::toggleFlag(kHasInherentRotation, value, m_context->flags);
}

void Bone::setFixedAxisEnable(bool value)
{
    internal::toggleFlag(kHasFixedAxis, value, m_context->flags);
}

void Bone::setLocalAxesEnable(bool value)
{
    internal::toggleFlag(kHasLocalAxes, value, m_context->flags);
}

void Bone::setTransformAfterPhysicsEnable(bool value)
{
    internal::toggleFlag(kTransformAfterPhysics, value, m_context->flags);
}

void Bone::setTransformedByExternalParentEnable(bool value)
{
    internal::toggleFlag(kTransformByExternalParent, value, m_context->flags);
}

void Bone::setInverseKinematicsEnable(bool value)
{
    m_context->enableInverseKinematics = value;
}

IBone *Bone::rootBoneRef() const
{
    return 0;
}

void Bone::setRootBoneRef(IBone * /* value */)
{
    /* do nothing */
}

IBone *Bone::effectorBoneRef() const
{
    return m_context->effectorBoneRef;
}

void Bone::setEffectorBoneRef(IBone *effector)
{
    if (!effector || (effector && effector->parentModelRef() == m_context->parentModelRef)) {
        m_context->effectorBoneRef = static_cast<Bone *>(effector);
        m_context->effectorBoneIndex = effector ? effector->index() : -1;
    }
}

int Bone::numIterations() const
{
    return m_context->numIterations;
}

void Bone::setNumIterations(int value)
{
    m_context->numIterations = value;
}

float32 Bone::angleLimit() const
{
    return m_context->angleLimit;
}

void Bone::setAngleLimit(float32 value)
{
    m_context->angleLimit = value;
}

void Bone::getJointRefs(Array<IKJoint *> &value) const
{
    const Array<DefaultIKJoint *> &joints = m_context->joints;
    const int njoints = joints.count();
    value.clear();
    value.reserve(njoints);
    for (int i = 0; i < njoints; i++) {
        DefaultIKJoint *joint = joints[i];
        value.append(joint);
    }
}

} /* namespace pmx */
} /* namespace VPVL2_VERSION_NS */
} /* namespace vpvl2 */
