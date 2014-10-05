#include "ReadWrite.hh"
#include "ModelSpace.hh"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include "string.h"

#define LINESIZE 400

using namespace std;

ModelSpace ReadWrite::ReadModelSpace( char* filename)
{

   ModelSpace modelspace;
   ifstream infile;
   stringstream ss;
   char line[LINESIZE];
   char cbuf[10][20];
   int ibuf[2];
   int n,l,j2,tz2,hvq;
   float fbuf[2];
   float spe,hw;
   int A;
   
   infile.open(filename);
   if ( not infile.good() )
   {
      cerr << "Trouble reading file " << filename << endl;
      return modelspace;
   }
   
   infile.getline(line,LINESIZE);

   while (!strstr(line,"Mass number") && !infile.eof())
   {
      infile.getline(line,LINESIZE);
   }
   ss << line;
   for (int i=0;i<10;i++) ss >> cbuf[i];
   ss >> A;
   ss.str(string()); // clear up the stringstream
   ss.clear();
   
   while (!strstr(line,"Oscillator length") && !infile.eof())
   {
      infile.getline(line,LINESIZE);
   }
   ss << line;
   ss >> cbuf[0] >> cbuf[1] >> cbuf[2] >> cbuf[3] >> fbuf[0] >> hw;
   modelspace.SetHbarOmega(hw);
   modelspace.SetTargetMass(A);
   cout << "Set hbar_omega to " << hw << endl;

   while (!strstr(line,"Legend:") && !infile.eof())
   {
      infile.getline(line,LINESIZE);
   }
   cout << "done reading header" << endl;
   while (!infile.eof())
   {
      infile >> cbuf[0] >> ibuf[0] >> n >> l >> j2 >> tz2
             >> ibuf[1] >> spe >> fbuf[0]  >> cbuf[1] >> cbuf[2];
      hvq = 0; // 0=hole 1=valence 2=particle outside the valence space
      if (strstr(cbuf[1],"particle")) hvq++;
      if (strstr(cbuf[2],"outside")) hvq++;
      modelspace.AddOrbit( Orbit(n,l,j2,tz2,hvq,spe) );
   
   }
   cout << "done reading interaction" << endl;
   
   infile.close();

   return modelspace;
}


void ReadWrite::ReadBareTBME( char* filename, Operator& Hbare)
{

  ifstream infile;
  char line[LINESIZE];
  int Tz,Par,J2,a,b,c,d;
  float fbuf[3];
  float tbme;

  infile.open(filename);
  if ( !infile.good() )
  {
     cerr << "Trouble reading file " << filename << endl;
     return;
  }

  infile.getline(line,LINESIZE);

  while (!strstr(line,"<ab|V|cd>") && !infile.eof()) // Skip lines until we see the header
  {
     infile.getline(line,LINESIZE);
  }

  int nline=0;
  while ( infile >> Tz >> Par >> J2  // Read tbme
                 >> a >> b >> c >> d
                 >> tbme >> fbuf[0] >> fbuf[1] >> fbuf[2] )
  {
     if (a==2 and b==6 and c==2 and d==6)
     {
        cout << "read TBME " << a << b << c << d << " J=" << J2/2 << " ("<<tbme<<")" << endl;
     }
     a--; b--; c--; d--; // Fortran -> C  => 1->0
     TwoBodyChannel h2body = Hbare.GetTwoBodyChannel(J2/2,Par,Tz);
     Ket * bra = Hbare.GetModelSpace()->GetKet(min(a,b),max(a,b));
     Ket * ket = Hbare.GetModelSpace()->GetKet(min(c,d),max(c,d));
     if (bra==NULL or ket==NULL) continue;
     tbme -= fbuf[2] * Hbare.GetModelSpace()->GetHbarOmega() / Hbare.GetModelSpace()->GetTargetMass();  // Some sort of COM correction. Check this
     float phase = 1.0;
     if (a==b) phase *= sqrt(2.);   // Symmetry factor. Confirm that this should be here
     if (c==d) phase *= sqrt(2.);   // Symmetry factor. Confirm that this should be here
     if (a>b) phase *= bra->Phase(J2/2);
     if (c>d) phase *= ket->Phase(J2/2);
     //h2body->SetTBME(bra,ket, phase*tbme); // can later change this to [bra,ket] to speed up.
     h2body.SetTBME(bra,ket, phase*tbme);
     //if (a==0 and b==11 and c==1 and d==10)
     if (a==0 and b==11 and c==10 and d==1)
     {
        cout << "Setting TBME " << a << b << c << d << " J=" << J2/2 << " to " << phase*tbme << " ("<<tbme<<")" << endl;
     }
     ++nline;
     cout << nline << endl;
  }

  return;
}


void ReadWrite::CalculateKineticEnergy(Operator *Hbare)
{
   int norbits = Hbare->GetModelSpace()->GetNumberOrbits();
   int A = Hbare->GetModelSpace()->GetTargetMass();
   float hw = Hbare->GetModelSpace()->GetHbarOmega();
   for (int a=0;a<norbits;a++)
   {
      Orbit * orba = Hbare->GetModelSpace()->GetOrbit(a);
      Hbare->OneBody(a,a) = 0.5 * hw * (2*orba->n + orba->l +3./2); 
      if (orba->n > 0)
      {
         int b = Hbare->GetModelSpace()->Index1(orba->n-1, orba->l, orba->j2, orba->tz2);
         Hbare->OneBody(a,b) = 0.5 * hw * sqrt( (orba->n)*(orba->n + orba->l +1./2));
         Hbare->OneBody(b,a) = Hbare->OneBody(a,b);
      }
   }
   Hbare->OneBody *= (1-1./A);

}


void ReadWrite::WriteOneBody(Operator& op, const char* filename)
{
   ofstream obfile;
   obfile.open(filename, ofstream::out);
   int norbits = op.GetModelSpace()->GetNumberOrbits();
   for (int i=0;i<norbits;i++)
   {
      for (int j=0;j<norbits;j++)
      {
         if (abs(op.GetOneBody(i,j)) > 1e-6)
         {
            obfile << i << "    " << j << "       " << op.GetOneBody(i,j) << endl;

         }
      }
   }
   obfile.close();
}



void ReadWrite::WriteTwoBody(Operator& op, const char* filename)
{
   ofstream tbfile;
   tbfile.open(filename, ofstream::out);
   ModelSpace * modelspace = op.GetModelSpace();
   for (int Tz=-1;Tz<=1;++Tz)
   {
      for (int p=0;p<=1;++p)
      {
         for (int J=0;J<JMAX;++J)
         {
            int npq = op.TwoBody[J][p][Tz+1].GetNumberKets();
            if (npq<1) continue;
            for (int i=0;i<npq;++i)
            {
               int iket = op.TwoBody[J][p][Tz+1].GetKetIndex(i);
               Ket *bra = modelspace->GetKet(iket);
               Orbit *oa = modelspace->GetOrbit(bra->p);
               Orbit *ob = modelspace->GetOrbit(bra->q);
               for (int j=i;j<npq;++j)
               {
                  int jket = op.TwoBody[J][p][Tz+1].GetKetIndex(j);
                  Ket *ket = modelspace->GetKet(jket);
                  //double tbme = TwoBody[J][p][Tz+1].TBME(i,j);
                  double tbme = op.TwoBody[J][p][Tz+1].GetTBME(bra,ket);
                  if ( abs(tbme)<1e-4 ) continue;
                  Orbit *oc = modelspace->GetOrbit(ket->p);
                  Orbit *od = modelspace->GetOrbit(ket->q);
                  int wint = 4;
                  int wfloat = 12;
/*                  tbfile 
                         << setw(wint)   << oa->n  << setw(wint) << oa->l  << setw(wint)<< oa->j2 << setw(wint) << oa->tz2 
                         << setw(wint+2) << ob->n  << setw(wint) << ob->l  << setw(wint)<< ob->j2 << setw(wint) << ob->tz2 
                         << setw(wint+2) << oc->n  << setw(wint) << oc->l  << setw(wint)<< oc->j2 << setw(wint) << oc->tz2 
                         << setw(wint+2) << od->n  << setw(wint) << od->l  << setw(wint)<< od->j2 << setw(wint) << od->tz2 
                         << setw(wint+3) << J << setw(wfloat) << std::fixed << tbme// << endl;
                         << "    < " << bra->p << " " << bra->q << " | V | " << ket->p << " " << ket->q << " >" << endl;
                         //<< setw(wint+2) << p   << setw(wint) << Tz << setw(wint) << J << setw(wfloat) << std::fixed << tbme << endl;
*/
                  tbfile 
                         << setw(wint) << bra->p
                         << setw(wint) << bra->q
                         << setw(wint) << ket->p
                         << setw(wint) << ket->q
                         << setw(wint+3) << J << setw(wfloat) << std::fixed << tbme// << endl;
                         << "    < " << bra->p << " " << bra->q << " | V | " << ket->p << " " << ket->q << " >" << endl;
                         //<< setw(wint+2) << p   << setw(wint) << Tz << setw(wint) << J << setw(wfloat) << std::fixed << tbme << endl;
               }
            }
         }
      }
   }
   tbfile.close();
}



