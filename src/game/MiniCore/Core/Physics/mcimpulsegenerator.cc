// This file belongs to the "MiniCore" game engine.
// Copyright (C) 2012 Jussi Lind <jussi.lind@iki.fi>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA  02110-1301, USA.
//

#include "mcimpulsegenerator.hh"
#include "mccontact.hh"
#include "../mcobject.hh"
#include "../mcshape.hh"

class MCImpulseGeneratorImpl
{
    void processContact(
        MCObject & pa, MCObject & pb, const MCContact & contact,
        const MCVector3dF & displacement, const MCVector3dF & linearImpulse,
        MCFloat restitution);

    MCContact * getDeepestInterpenetration(const std::vector<MCContact *> & contacts);

    void generateImpulsesFromDeepestContacts(MCObject & object);

    friend class MCImpulseGenerator;
};

MCContact * MCImpulseGeneratorImpl::getDeepestInterpenetration(
    const std::vector<MCContact *> & contacts)
{
    MCFloat maxDepth = 0;
    MCContact * bestContact = nullptr;
    for (MCContact * contact : contacts)
    {
        if (contact->interpenetrationDepth() > maxDepth)
        {
            maxDepth = contact->interpenetrationDepth();
            bestContact = contact;
        }
    }
    return bestContact;
}

void MCImpulseGeneratorImpl::processContact(
    MCObject & pa, MCObject & pb, const MCContact & contact,
    const MCVector3dF & displacement, const MCVector3dF & linearImpulse,
    MCFloat restitution)
{
    const MCFloat invMassA = pa.invMass();
    const MCFloat invMassB = pb.invMass();
    const MCFloat invInerA = pa.invMomentOfInertia();
    const MCFloat invInerB = pb.invMomentOfInertia();

    if (!pa.stationary())
    {
        const MCVector3dF & contactPoint(contact.contactPoint());
        const MCVector3dF arm = contactPoint - pa.location();

        // Linear component
        const MCFloat massScaling = invMassA / (invMassA + invMassB);
        pa.displace(displacement * massScaling);

        // This ad-hoc scaling affects the balance between linear and angular components.
        MCFloat linearBalance = 1.0f;
        if (pa.shape())
        {
            const MCFloat d = pa.shape()->radius() * 2;
            MCFloat linearBalance = 1.0f - arm.lengthFast() / d;
            linearBalance = linearBalance < 0 ? 0 : linearBalance;
        }

        const MCFloat effRestitution = 1.0f + restitution;
        pa.addLinearImpulse(
            linearImpulse * effRestitution * massScaling * linearBalance);

        // Angular component
        const MCVector3dF rotationalImpulse =
            MCVector3dF(linearImpulse * pa.mass()) % arm / pa.momentOfInertia();

        const MCFloat magnitude   = rotationalImpulse.k();
        const MCFloat inerScaling = invInerA / (invInerA + invInerB);
        pa.addRotationalImpulse(-magnitude * effRestitution * inerScaling);
    }
}

void MCImpulseGeneratorImpl::generateImpulsesFromDeepestContacts(MCObject & object)
{
    auto iter(object.contacts().begin());
    for (; iter != object.contacts().end(); iter++)
    {
        const MCContact * contact = getDeepestInterpenetration(iter->second);
        if (contact)
        {
            MCObject & pa(object);
            MCObject & pb(contact->object());

            const MCFloat restitution(
                std::min(pa.restitution(), pb.restitution()));

            const MCVector2dF velocityDelta(pb.velocity() - pa.velocity());

            const MCVector3dF linearImpulse(
                contact->contactNormal() * contact->contactNormal().dot(velocityDelta));

            const MCVector3dF displacement(
                contact->contactNormal() * contact->interpenetrationDepth());

            processContact(pa, pb, *contact, displacement, linearImpulse, restitution);

            processContact(pb, pa, *contact, -displacement, -linearImpulse, restitution);

            // Remove contact with pa from pb, because it was already handled here.
            pb.deleteContacts(pa);
        }
    }

    object.deleteContacts();
}

MCImpulseGenerator::MCImpulseGenerator()
: m_pImpl(new MCImpulseGeneratorImpl)
{}

MCImpulseGenerator::~MCImpulseGenerator()
{
    delete m_pImpl;
}

void MCImpulseGenerator::generateImpulsesFromDeepestContacts(MCObject & object)
{
    m_pImpl->generateImpulsesFromDeepestContacts(object);
}
