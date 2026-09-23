#include "EDM4hepFrameBuilder.h"
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <edm4hep/EventHeaderCollection.h>

podio::Frame buildEDM4hepFrame(
    const EDM4hepMergedCollections&  col,
    const std::vector<std::string>&  tracker_names,
    const std::vector<std::string>&  calo_names,
    const std::vector<std::string>&  gp_names)
{
    podio::Frame frame;

    // --- EventHeader ---
    edm4hep::EventHeaderCollection eh_coll;
    for (const auto& hd : col.event_headers) {
        auto h = eh_coll.create();
        h.setEventNumber(hd.eventNumber);
        h.setRunNumber(hd.runNumber);
        h.setTimeStamp(hd.timeStamp);
        h.setWeight(hd.weight);
    }
    frame.put(std::move(eh_coll), "EventHeader");

    // --- SubEventHeaders ---
    edm4hep::EventHeaderCollection sub_eh_coll;
    for (const auto& hd : col.sub_event_headers) {
        auto h = sub_eh_coll.create();
        h.setEventNumber(hd.eventNumber);
        h.setRunNumber(hd.runNumber);
        h.setTimeStamp(hd.timeStamp);
        h.setWeight(hd.weight);
    }
    frame.put(std::move(sub_eh_coll), "SubEventHeaders");

    // --- MCParticles: two-pass to restore parent/daughter links ---
    edm4hep::MCParticleCollection mc_coll;
    std::vector<edm4hep::MutableMCParticle> mutable_particles;
    mutable_particles.reserve(col.mcparticles.size());

    for (const auto& pd : col.mcparticles) {
        auto mp = mc_coll.create();
        mp.setPDG(pd.PDG);
        mp.setGeneratorStatus(pd.generatorStatus);
        mp.setSimulatorStatus(pd.simulatorStatus);
        mp.setCharge(pd.charge);
        mp.setTime(pd.time);
        mp.setMass(pd.mass);
        mp.setVertex(pd.vertex);
        mp.setEndpoint(pd.endpoint);
        mp.setMomentum(pd.momentum);
        mp.setMomentumAtEndpoint(pd.momentumAtEndpoint);
        mp.setHelicity(pd.helicity);
        mutable_particles.push_back(mp);
    }
    for (size_t i = 0; i < col.mcparticles.size(); ++i) {
        const auto& pd = col.mcparticles[i];
        auto& mp = mutable_particles[i];
        for (unsigned j = pd.parents_begin; j < pd.parents_end; ++j) {
            int idx = col.mcparticle_parents_refs[j].index;
            if (idx >= 0 && idx < static_cast<int>(mutable_particles.size()))
                mp.addToParents(mutable_particles[idx]);
        }
        for (unsigned j = pd.daughters_begin; j < pd.daughters_end; ++j) {
            int idx = col.mcparticle_daughters_refs[j].index;
            if (idx >= 0 && idx < static_cast<int>(mutable_particles.size()))
                mp.addToDaughters(mutable_particles[idx]);
        }
    }
    frame.put(std::move(mc_coll), "MCParticles");

    // --- SimTrackerHits ---
    for (const auto& name : tracker_names) {
        edm4hep::SimTrackerHitCollection trk_coll;
        const auto& hits      = col.tracker_hits.at(name);
        const auto& part_refs = col.tracker_hit_particle_refs.at(name);
        for (size_t i = 0; i < hits.size(); ++i) {
            const auto& hd = hits[i];
            auto hit = trk_coll.create();
            hit.setCellID(hd.cellID);
            hit.setEDep(hd.eDep);
            hit.setTime(hd.time);
            hit.setPathLength(hd.pathLength);
            hit.setQuality(hd.quality);
            hit.setPosition(hd.position);
            hit.setMomentum(hd.momentum);
            if (i < part_refs.size()) {
                int idx = part_refs[i].index;
                if (idx >= 0 && idx < static_cast<int>(mutable_particles.size()))
                    hit.setParticle(mutable_particles[idx]);
            }
        }
        frame.put(std::move(trk_coll), name);
    }

    // --- SimCalorimeterHits + CaloHitContributions ---
    for (const auto& name : calo_names) {
        const auto& c_data  = col.calo_contributions.at(name);
        const auto& c_prefs = col.calo_contrib_particle_refs.at(name);

        edm4hep::CaloHitContributionCollection contrib_coll;
        std::vector<edm4hep::MutableCaloHitContribution> mutable_contribs;
        mutable_contribs.reserve(c_data.size());
        for (size_t j = 0; j < c_data.size(); ++j) {
            const auto& cd = c_data[j];
            auto contrib = contrib_coll.create();
            contrib.setPDG(cd.PDG);
            contrib.setEnergy(cd.energy);
            contrib.setTime(cd.time);
            contrib.setStepPosition(cd.stepPosition);
            if (j < c_prefs.size()) {
                int pidx = c_prefs[j].index;
                if (pidx >= 0 && pidx < static_cast<int>(mutable_particles.size()))
                    contrib.setParticle(mutable_particles[pidx]);
            }
            mutable_contribs.push_back(contrib);
        }

        edm4hep::SimCalorimeterHitCollection calo_coll;
        const auto& hits = col.calo_hits.at(name);
        for (const auto& hd : hits) {
            auto hit = calo_coll.create();
            hit.setCellID(hd.cellID);
            hit.setEnergy(hd.energy);
            hit.setPosition(hd.position);
            for (unsigned j = hd.contributions_begin; j < hd.contributions_end; ++j) {
                if (j < mutable_contribs.size())
                    hit.addToContributions(mutable_contribs[j]);
            }
        }
        frame.put(std::move(contrib_coll), name + "Contributions");
        frame.put(std::move(calo_coll), name);
    }

    // --- GP parameters ---
    for (const auto& branch_name : gp_names) {
        const auto& keys_vec = col.gp_key_branches.at(branch_name);
        if (branch_name.find("GPIntKeys") != std::string::npos) {
            for (size_t ei = 0; ei < keys_vec.size() && ei < col.gp_int_values.size(); ++ei)
                frame.putParameter(keys_vec[ei], col.gp_int_values[ei]);
        } else if (branch_name.find("GPFloatKeys") != std::string::npos) {
            for (size_t ei = 0; ei < keys_vec.size() && ei < col.gp_float_values.size(); ++ei)
                frame.putParameter(keys_vec[ei], col.gp_float_values[ei]);
        } else if (branch_name.find("GPDoubleKeys") != std::string::npos) {
            for (size_t ei = 0; ei < keys_vec.size() && ei < col.gp_double_values.size(); ++ei)
                frame.putParameter(keys_vec[ei], col.gp_double_values[ei]);
        } else if (branch_name.find("GPStringKeys") != std::string::npos) {
            for (size_t ei = 0; ei < keys_vec.size() && ei < col.gp_string_values.size(); ++ei)
                frame.putParameter(keys_vec[ei], col.gp_string_values[ei]);
        }
    }

    return frame;
}
