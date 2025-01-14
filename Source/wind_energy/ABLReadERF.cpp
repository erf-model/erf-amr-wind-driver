#include <amr-wind/wind_energy/ABLBoundaryPlane.H>
#include "ABLReadERF.H"

void fill_amrwind_bndry(
  amrex::Vector<amrex::BndryRegister*>& bndry,
  MultiBlockContainer* mbc)
{
    amrex::Vector<amrex::Real> erf_times; erf_times.resize(2);
    mbc->PopulateErfTimesteps(erf_times.data());
    //const int time_idx = old_bndry ? 0 : 1;

    for (const auto& fld : mbc->get_amrwind().repo().fields())
    {
      const auto& field = *fld;
      for (amrex::OrientationIter oit; oit != nullptr; ++oit)
      {
        auto ori = oit();
        if (((field.bc_type()[ori] == BC::mass_inflow)
          || (field.bc_type()[ori] == BC::mass_inflow_outflow))) {
          if (field.name() == "temperature") {
            mbc->CopyERFtoAMRWindBoundaryReg(*bndry[1], ori, erf_times[1], field.name());
          } else if (field.name() == "velocity") {
            mbc->CopyERFtoAMRWindBoundaryReg(*bndry[0], ori, erf_times[1], field.name());
          }
        }
      } // ori
    } // fields

    mbc->old_bndry_time = mbc->new_bndry_time;
    amrex::Print() << "Setting old bndryreg time to be " << mbc->old_bndry_time << std::endl;
    mbc->new_bndry_time = erf_times[1];
    amrex::Print() << "Setting new bndryreg time to be " << mbc->new_bndry_time << std::endl;
}

void read_erf(const amrex::Real time,
              amrex::Vector<amrex::Real>& m_in_times,
              amr_wind::InletData& m_in_data,
              const amrex::Vector<amr_wind::Field*>& m_fields,
              MultiBlockContainer* mbc)
{
  //amrex::Print() << " IN READ_ERF " << std::endl;
  //amrex::Print() << "    TIME IS " <<  time << std::endl;

  // return early if current erf data can still be interpolated in time
  if ((m_in_data.tn() <= time) && (time < m_in_data.tnp1())) {
    m_in_data.interpolate(time);
    return;
  }

  const int lev = 0;
  // Get current ERF time values
  mbc->PopulateErfTimesteps(m_in_times.data());

  // for reading erf data during amr-wind initialization
  if (m_in_times[0] < 0.) {

    //amrex::Print() << "OLD TIME FROM ERF IS " <<  m_in_times[0] << std::endl;
    //amrex::Print() << "NEW TIME FROM ERF IS " <<  m_in_times[1] << std::endl;

    AMREX_ALWAYS_ASSERT((m_in_times[0]<= time) && (time <= m_in_times[1]));

    for (auto* fld : m_fields) {

      auto& field = *fld;
      const auto& geom = field.repo().mesh().Geom();
      amrex::Box domain = geom[lev].Domain();
      amrex::BoxArray ba(domain);
      amrex::DistributionMapping dm{ba};

      const int in_rad = 1;
      const int out_rad = 1;
      const int extent_rad = 0;

      amrex::BndryRegister bndry1(ba, dm, in_rad, out_rad, extent_rad, field.num_comp());
      amrex::BndryRegister bndry2(ba, dm, in_rad, out_rad, extent_rad, field.num_comp());

      for (amrex::OrientationIter oit; oit != nullptr; ++oit)
      {
        auto ori = oit();
        if ((!m_in_data.is_populated(ori)) ||
            ((field.bc_type()[ori] != BC::mass_inflow) &&
             (field.bc_type()[ori] != BC::mass_inflow_outflow))) {
          continue;
        }
        if (time >= 0.0) {
          if (field.name() == "temperature") {
            mbc->CopyERFtoAMRWindBoundaryReg(bndry1, ori, m_in_times[0], field.name());
            mbc->CopyERFtoAMRWindBoundaryReg(bndry2, ori, m_in_times[1], field.name());
          } else if (field.name() == "velocity") {
            mbc->CopyERFtoAMRWindBoundaryReg(bndry1, ori, m_in_times[0], field.name());
            mbc->CopyERFtoAMRWindBoundaryReg(bndry2, ori, m_in_times[1], field.name());
          }
        }
          m_in_data.read_data_native(oit, bndry1, bndry2, lev, fld, time, m_in_times);
      } // oit
    } // fields
    m_in_data.interpolate(time);

  } else {
  // regular timestepping

    // Set old and new time to be the old and new time of the AMR-Wind step.
    m_in_times[0] = mbc->old_bndry_time;
    m_in_times[1] = mbc->new_bndry_time;

    //amrex::Print() << "    TIME TO FILL          IS " << time << std::endl;
    //amrex::Print() << "OLD TIME FROM BNDRY_TIMES IS " <<  m_in_times[0] << std::endl;
    //amrex::Print() << "NEW TIME FROM BNDRY_TIMES IS " <<  m_in_times[1] << std::endl;

    amrex::Real one_plus_eps  = 1.0 + 1.e-8;
    AMREX_ALWAYS_ASSERT((m_in_times[0] <= time*one_plus_eps) && (time <= m_in_times[1]*one_plus_eps));

    for (auto* fld : m_fields)
    {
      auto& field = *fld;
      for (amrex::OrientationIter oit; oit != nullptr; ++oit)
      {
        auto ori = oit();
        if ((!m_in_data.is_populated(ori)) ||
            ((field.bc_type()[ori] != BC::mass_inflow) &&
             (field.bc_type()[ori] != BC::mass_inflow_outflow))) {
          continue;
        }
        if (field.name() == "temperature") {
          m_in_data.read_data_native(oit, *(mbc->bndry1[1]), *(mbc->bndry2[1]), lev, fld, time, m_in_times);
        } else if (field.name() == "velocity") {
          m_in_data.read_data_native(oit, *(mbc->bndry1[0]), *(mbc->bndry2[0]), lev, fld, time, m_in_times);
        }
      } // ori
    } // fields
    m_in_data.interpolate(time);
  } // initialization vs regular timestepping
}
