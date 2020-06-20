 /*   COVID-19 Sicily data plotter - Francesco Barone
 /  WARNING: use this code in ROOT! Otherwise it won't work.  */

#include <stdio.h>
#include <stdlib.h>

 //structure of files/directories
#define FDIRNAME     "datas"
#define FDIRNAME_REG "dati-regioni"
#define FDIRNAME_PRV "dati-province"
#define FILEBASENAME "dpc-covid19-ita-"
#define PDIRNAME     "canvas/"
#define ODIRNAME     "online/"
#define LASTUPHTML   "lastupdate.html"
#define CFG_FILE     "covidd.cfg"

 //GitHub DPC
#define GIT_BASELNK  "https://raw.githubusercontent.com/pcm-dpc/COVID-19/master/"
#define F_REG_CSV    "dpc-covid19-ita-regioni.csv"
#define F_PRV_CSV    "dpc-covid19-ita-province.csv"
#define F_NTL_CSV    "dpc-covid19-ita-andamento-nazionale.csv"

#define REGION_ID    19  //id of Sicily
#define PRV_FIRST_ID 81  //first region id: Trapani 081 in province-CSV
#define REGION_FIELD 2   //region_id field number in .CSV  //start count from zero!
#define N_REGIONS    20  //number of regions in Italy

#define TESTED_FIELDS 16  //CSV fields structure may change, so we check how many fields...
    /*last updated: 21.04.2020 (CSV fields changed from 15 to 16) 
                    30.03.2020 (CSV fields changed from 14 to 15) */
    
#define DAYS_TO_PRINT  15  //how many days to print for secondary graphs
#define DAYS_TO_PRINT2 10  
#define GR2MARK 1.2        //marker sizes
#define GR3MARK 1.2

 //additional
#define STRLEN  512
#define STRLEN2 20
#define BRED    "\e[1;31m"
#define YEL     "\e[0;33m"
#define cclear  "\e[0m"

typedef struct _date { unsigned int mm, dd, yy; } date;
void increment_date(date* day)
 {
  unsigned int day_of_months[12] =  //used for date logic
   {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; 
  if(((day->dd)++)>=day_of_months[day->mm-1]) 
   {  if((day->mm!=2)||(day->yy%4!=0)||(day->dd!=day_of_months[1]+1)) {day->dd=1; day->mm++;}  }
  if(day->mm>12) { day->mm=1; day->yy++; }
 }

typedef struct _exdata 
 { TH1I *synt, *intensive, *home, *pos, *deltapos, *healed, *deads; } exdata;

typedef struct _root_block
 { exdata* hst; TH1I** hpr; THStack *h_ngeneral, *h_rgeneral;
   TLegend *legend, *legendsick, *legendprv;  TText T;  } root_block;

typedef struct _setting_block
 {  unsigned int n_days, n_fields, status, cfg;  date iday, fday; } setting_block;

void download(void)
 { //this function downloads last files from GitHub DPC
  system("cd " FDIRNAME " && wget -N " GIT_BASELNK "dati-regioni/" F_REG_CSV);
  system("cd " FDIRNAME " && wget -N " GIT_BASELNK "dati-province/" F_PRV_CSV);
  system("cd " FDIRNAME " && wget -N " GIT_BASELNK "dati-andamento-nazionale/" F_NTL_CSV);
  printf("\n\n\n <<DOWNLOAD SEQUENCE COMPLETED>>\n\n");
 }

void writehtml(date* daytoprint)
 {
  FILE* pf;
  if((pf = fopen( ODIRNAME LASTUPHTML,"w"))!=NULL)
   {
    fprintf(pf,"<!DOCTYPE html>\n<html> <body>\n");
    fprintf(pf,"<p>Ultimo aggiornamento:</p>\n");
    fprintf(pf,"<h3> %02d.%02d.%04d</h3>\n",daytoprint->dd,daytoprint->mm,daytoprint->yy);
    fprintf(pf,"</body> </html> ");
    fclose(pf);
   }
  else printf(BRED "  Error " cclear "in HTML opening!\n");
 } 

void skip_fields(unsigned int* ind, char* line, unsigned int n_skip)
 {  //this function moves index (in line) of a 'n_skip' number of fields
  do { if(line[(*ind)++]==',') n_skip--; }
   while( (n_skip!=0) && (line[*ind]!='\0') );
 }

int read_int_field(unsigned int* ind, char* line)
 {  //this function reads int value from line, starting from index position
  char field_string[STRLEN2];      unsigned int i=0;
  while((line[*ind + i]!=',')&&(line[*ind + i]!='\0'))
   { field_string[i]=line[*ind+i];  i++;  };
  field_string[i]='\0';
  return atoi(field_string);
 }

int assign(char* input, char const* ident, char const* corrsp)
 {
  int i = 0;
  while(ident[i]!='\0')
   { if(input[0]==ident[i]) return corrsp[i]-'0'; i++;   }
  return -1;
 }

int compare_strings(char* str1, char* str2)
 {
  unsigned int i = 0;
  do { if(str1[i]!=str2[i]) return 0; i++;  } while(str1[i]!='\0' && str1[i]!='\n');
  return 1;
 }

int test_files(setting_block* sb)
 {
  FILE* pf;  char str[STRLEN];  

  printf("\n > checking setting file ...");
  if((pf=fopen(CFG_FILE,"r"))==NULL)
   { printf(" no file. A new .cfg file will be created next elaboration!\n"); }
  else { printf("ok\n"); fclose(pf); sb->cfg=1; }

  printf(" > checking db files ...");

  sprintf(str,FDIRNAME"/" F_NTL_CSV);   
  if((pf=fopen(str,"r"))==NULL) { printf(" error opening " F_NTL_CSV "\n"); return 0;  }
  else fclose(pf);

  sprintf(str,FDIRNAME"/" F_REG_CSV);
  if((pf=fopen(str,"r"))==NULL) { printf(" error opening " F_REG_CSV "\n"); return 0;  }
  else fclose(pf);

  sprintf(str,FDIRNAME"/" F_PRV_CSV);
  if((pf=fopen(str,"r"))==NULL) { printf(" error opening " F_PRV_CSV "\n"); return 0;  }
  else fclose(pf);
  printf("ok\n"); 
  


  //sb->cfg = 0;

  return 1;
 }



//PLOT DEFINITIONS:
TCanvas* plot_general(int id, root_block* rb)
 {  //this function plots a general graph of selected datas: id=0 for ITALY and id=1 for SICILY
  char eti[30];
  exdata* hst = rb->hst;   TLegend* legend1 = rb->legend;  
  THStack* h_stack;        TLegend* legend2 = rb->legendsick;
  if(id==0) h_stack = rb->h_ngeneral; else h_stack = rb->h_rgeneral;

  if(id==0) sprintf(eti,"CoViD Italy general"); else sprintf(eti,"CoViD Sicily general");
  TCanvas *canva = new TCanvas(eti,eti,100,100,1400,800);

  canva->SetGrid();  //canva->Draw();
  canva->SetTopMargin(0.15);    
    TPad *pb1 = new TPad("pb1","pb1",0.00,0.5,1.0,1.);  pb1->Draw(); pb1->Divide(1,1);
  TPad *pb11 = (TPad*)pb1->cd(1);   pb11->Draw();  
   canva->cd(1); h_stack->Draw();  legend1->Draw();

  (rb->T).SetTextFont(60);   
  if(id==1) (rb->T).DrawTextNDC(.5,.95,"CoViD-19 [Sicilia]");  
  else (rb->T).DrawTextNDC(.5,.95,"CoViD-19 [Italia]");   
  (rb->T).SetTextFont(40);
  canva->cd(0);

  TPad *pb2 = new TPad("pb1","pb1",0.00,0.,1.0,0.5);
  pb2->Draw();  pb2->Divide(2,1);  TPad *pb21=(TPad*)pb2->cd(1);   pb21->Draw();   canva->cd(1);  
   hst[id].deltapos->Draw("TEXT60");
  (rb->T).DrawTextNDC(.22,.92,"Nuovi positivi al giorno (ultimi 30gg)");

  TPad *pb22 = (TPad*)pb2->cd(2);  pb22->Draw(); canva->cd(1);         
  hst[id].home->Draw("bar0,TEXT90"); 
  hst[id].synt->Draw("bar0,same,TEXT90");
  hst[id].intensive->Draw("bar0,same,TEXT90");
  (rb->T).DrawTextNDC(.3,.92,"Ripartizione dei positivi (ultimi 15gg)");    legend2->Draw();
  return canva;
 }

TCanvas* plot_single_1(int id, root_block* rb)
 {  //this function plots graph of selected region whole cases
  char eti[30];
  TLegend* legend1 = rb->legend;  
  THStack* h_stack;
  if(id==0) h_stack = rb->h_ngeneral; else h_stack = rb->h_rgeneral;

  if(id==0) sprintf(eti,"CoViD Italy report"); else sprintf(eti,"CoViD Sicily report");
  TCanvas *canva = new TCanvas(eti,eti,100,100,1200,600);
  canva->SetGrid();  //canva->Draw();
  canva->SetTopMargin(0.15);    
  canva->cd(1); h_stack->Draw();  legend1->Draw();

  (rb->T).SetTextFont(60);   
  if(id==1) (rb->T).DrawTextNDC(.5,.95,"andamento [Sicilia]");  
  else (rb->T).DrawTextNDC(.5,.95,"andamento [Italia]");   
  return canva;
 }

TCanvas* plot_single_2(int id, root_block* rb)
 {  //this function plots graph of selected region new cases
  char eti[30];
  exdata* hst = rb->hst;

  if(id==0) sprintf(eti,"CoViD Italy new cases"); else sprintf(eti,"CoViD Sicily new cases");
  TCanvas *canva = new TCanvas(eti,eti,100,800,900,600);
  canva->SetTopMargin(0.15);    
  canva->cd(1); hst[id].deltapos->Draw("TEXT60"); (rb->T).SetTextFont(60);
  (rb->T).DrawTextNDC(.3,.92,"Nuovi positivi al giorno (ultimi 30gg)");   
  return canva;
 }

TCanvas* plot_single_3(int id, root_block* rb)
 {  //this function plots graph of selected region positives partition
  char eti[30];
  exdata* hst = rb->hst;

  if(id==0) sprintf(eti,"CoViD Italy postives"); else sprintf(eti,"CoViD Sicily positives");
  TCanvas *canva = new TCanvas(eti,eti,800,800,1000,600);
  canva->SetTopMargin(0.15);   canva->cd(1); 
  hst[id].home->Draw("bar0,TEXT90"); 
  hst[id].synt->Draw("bar0,same,TEXT90");
  hst[id].intensive->Draw("bar0,same,TEXT90");
  (rb->T).DrawTextNDC(.3,.92,"Ripartizione dei positivi (ultimi 15gg)");    (rb->legendsick)->Draw();
  return canva;
 }

TCanvas* plot_province(root_block* rb)
 {  //this function plots graph of total cases by province
  char eti[30];  int i;  sprintf(eti,"CoViD Sicily provinces cases");
  TH1I** prov = rb->hpr;

  TCanvas *canva = new TCanvas(eti,eti,800,800,1600,700);
  canva->SetTopMargin(0.15);   canva->cd(1); 
  prov[0]->Draw("bar0,TEXT90"); 
  for(i=1; i<9; i++) prov[i]->Draw("bar0,same,TEXT90");
  (rb->T).DrawTextNDC(.3,.92,"Totali casi delle province in Sicilia (ultimi 10gg)");    (rb->legendprv)->Draw();
  return canva;
 }







setting_block database_info(setting_block *old_sb)
 {
  int i,j;          unsigned int n_fields;
  FILE *pfn, *pfr, *pfp, *pfs;     
  char str[STRLEN], str2[STRLEN], daystr[STRLEN2], ch;

  setting_block sb;  sb.cfg=(old_sb->cfg);

  /*
  pfn = popen("ls " FDIRNAME" | wc -l", "r");
  if(fgets(str, sizeof(str), pfn)==NULL) printf("ERR");     
  pclose(pfn); 
  if((n_days=atoi(str))<3) 
   { printf(" No files in directory: forced download attempt.\n");  download(); }
  else
   { //asks to download new datas
    printf("\n You want to download new datas from " YEL "GitHub" cclear "? [y/n] > ");
    scanf("%s",str);        if(str[0]=='Y' || str[0]=='y') download();
   } */
  
  //opens NATIONAL file
  sprintf(str,FDIRNAME"/" F_NTL_CSV);   printf(" > opening: %s\n",str);
  if((pfn=fopen(str,"r"))==NULL) { printf("ERROR opening %s",str);  } 
  //opens REGIONAL file 
  sprintf(str,FDIRNAME"/" F_REG_CSV);   printf(" > opening: %s\n",str);
  if((pfr=fopen(str,"r"))==NULL) { printf("ERROR opening %s",str);  } 
  //opens PROVINCE file
  sprintf(str,FDIRNAME"/" F_PRV_CSV);   printf(" > opening: %s\n",str);
  if((pfp=fopen(str,"r"))==NULL) { printf("ERROR opening %s",str);  }
  //reads SETTING file
  if((pfs=fopen(CFG_FILE,"r"))==NULL && (old_sb->cfg==1)) 
   { printf("unexpected ERROR in config file\n"); sb.cfg=old_sb->cfg=0; }
  
  if(old_sb->cfg==1)
   {
    
    fscanf(pfn,"%s", str);  fscanf(pfs,"%s", str2);
    if(!compare_strings(str,str2)) sb.cfg=3;

    fscanf(pfr,"%s", str);  fscanf(pfs,"%s", str2);
    if(!compare_strings(str,str2)) sb.cfg=3;

    fscanf(pfp,"%s", str);  fscanf(pfs,"%s", str2);
    if(!compare_strings(str,str2)) sb.cfg=3;
    //rewind national file: 
    fclose(pfn); sprintf(str,FDIRNAME"/" F_NTL_CSV);  
    if((pfn=fopen(str,"r"))==NULL) { printf("ERROR opening %s",str);  } 
    else fscanf(pfn,"%s", str);
    //old_sb->cfg=0;  //force check of n_fields
   } 
  if(old_sb->cfg==0 || sb.cfg==3)
   {
    //counts number of csv fields from NATIONAL file
    fscanf(pfn,"%s", str);       j=n_fields=0;
    do{ if(str[j]==',') n_fields++;  } while(str[j++]!='\0');
    printf("   number of Nfields detected: %u  ",++n_fields);
    if(n_fields!=TESTED_FIELDS) 
     { if(sb.cfg==3) old_sb->cfg=3;
       printf(BRED "  not matching!\n" cclear); }
    else printf(" ...OK (so far)\n");

    //counts number of csv fields from NATIONAL file
    fscanf(pfr,"%s", str);       j=i=0;
    do{ if(str[j]==',') i++;  } while(str[j++]!='\0');
    printf("   number of Rfields detected: %u  ",++i);
    if(i!=TESTED_FIELDS+4)
     { if(sb.cfg==3) old_sb->cfg=3; 
       printf(BRED "  not matching!\n" cclear); }
    else printf(" ...OK (so far)\n");
  
    //counts number of csv fields from PROVINCE file
    fscanf(pfp,"%s", str);       j=i=0;
    do{ if(str[j]==',') i++;  } while(str[j++]!='\0');
    printf("   number of Pfields detected: %u  ",++i);
    if(i!=TESTED_FIELDS-3)
     { if(sb.cfg==3) old_sb->cfg=3; 
       printf(BRED "  not matching!\n" cclear); }
    else printf(" ...OK (so far)\n");
   }
  fclose(pfr); fclose(pfp);
  if(old_sb->cfg==3 && sb.cfg==3)
   { printf(BRED "file structure has definitely changed!\n" cclear); 
     printf("Suggesting to set TESTED_FIELDS to %u\n",n_fields); }
  else if(old_sb->cfg==1 && sb.cfg==3) 
   printf(BRED "file structure may have changed!" cclear " (wrong cfg match)\n");
  
  //getting first day of datas from NATIONAL file
  fscanf(pfn,"%s",str);     daystr[0]=str[0];  daystr[1]=str[1];
  daystr[2]=str[2];  daystr[3]=str[3];  daystr[4]='\0';  sb.iday.yy=atoi(daystr);
  daystr[0]=str[5];  daystr[1]=str[6];  daystr[2]='\0';  sb.iday.mm=atoi(daystr);
  daystr[0]=str[8];  daystr[1]=str[9];  daystr[2]='\0';  sb.iday.dd=atoi(daystr);
  
  printf(" first day of datas: %02u-%02u-%4u\n", sb.iday.dd, sb.iday.mm, sb.iday.yy);
  sprintf(daystr,"%02u-%02u",sb.iday.dd,sb.iday.mm);

  //counting number of data days available in files...
  sb.n_days=0; sb.fday=sb.iday;
  while ((ch = fgetc(pfn)) != EOF)
    {
     if(ch=='\n') 
      { if(sb.n_days!=0) increment_date(&(sb.fday));  (sb.n_days)++;  }  
    }
  fclose(pfn);     //sb.n_days--;
  printf(" detected %u days of data\n",sb.n_days);
  printf(" final day of datas: %02u-%02u-%4u\n", sb.fday.dd, sb.fday.mm, sb.fday.yy);
  
  if(sb.cfg==3) sb.cfg=0; //next elaboration cfg file will be overwritten!
  sb.n_fields=n_fields;  sb.status = 1;  return sb;
 }


void elaborate(root_block* rb, setting_block* sb)
 {
  int i,j,k,max_prov,temp_prov;     unsigned int index = 0; 
  FILE *pfn, *pfr, *pfp, *pfs;
  char str[STRLEN], daystr[STRLEN2], ch;

  char labs[2][4] = {"ITA","SIC"};

  date c_day = sb->iday; //current day,

   //just to keep notation simple...
  exdata* hst = (exdata*)malloc(sizeof(exdata)*2);
  for(i=0; i<2; i++) hst[i]=rb->hst[i];
  TH1I** hpr = (TH1I**)malloc(sizeof(TH1I*)*9);
  for(i=0; i<9; i++) hpr[i]=rb->hpr[i];

  //ROOT GRAPH OPERATIONS:
   //histogram stacks
  rb->h_ngeneral = new THStack("ITA general","");
  rb->h_rgeneral = new THStack("Sicily general","");
  
   //datas
  for(i=0; i<2; i++) //initializing...
   {
    sprintf(str,"%s-hospital syntomatics",labs[i]);
    hst[i].synt      = new TH1I(str,"",DAYS_TO_PRINT,0,DAYS_TO_PRINT);
    hst[i].synt->SetFillColor(kOrange);
    
    sprintf(str,"%s-hospital intensive therapy",labs[i]);
    hst[i].intensive = new TH1I(str,"",DAYS_TO_PRINT,0,DAYS_TO_PRINT);
    hst[i].intensive->SetFillColor(kOrange+8);

    sprintf(str,"%s-home therapy",labs[i]);
    hst[i].home      = new TH1I(str,"",DAYS_TO_PRINT,0,DAYS_TO_PRINT);
    hst[i].home->SetFillColor(kCyan+1);

    sprintf(str,"%s-total positives",labs[i]);
    hst[i].pos       = new TH1I(str,"",sb->n_days,0,sb->n_days);
    hst[i].pos->SetFillColor(kYellow-4);     hst[i].pos->SetCanExtend(TH1::kAllAxes);

    sprintf(str,"%s-delta positives",labs[i]);
    hst[i].deltapos  = new TH1I(str,"",2*DAYS_TO_PRINT,0,2*DAYS_TO_PRINT);
    hst[i].deltapos->SetFillColor(kBlue); 

    sprintf(str,"%s-healed",labs[i]);
    hst[i].healed    = new TH1I(str,"",sb->n_days,0,sb->n_days);
    hst[i].healed->SetFillColor(kGreen-4); hst[i].healed->SetCanExtend(TH1::kAllAxes);

    sprintf(str,"%s-deads",labs[i]);
    hst[i].deads     = new TH1I(str,"",sb->n_days,0,sb->n_days);
    hst[i].deads->SetFillColor(kRed-4);    hst[i].deads->SetCanExtend(TH1::kAllAxes);
    
    //some esthetic refinements
    hst[i].home->SetMarkerSize(GR3MARK);       hst[i].home->SetMinimum(0);    
    hst[i].home->SetBarWidth(0.25);  hst[i].home->SetBarOffset(0.05);  hst[i].home->SetStats(0); 
    hst[i].home->GetYaxis()->SetTickLength(-0.01); 
 
    hst[i].synt->SetMarkerSize(GR3MARK);       hst[i].synt->SetBarWidth(0.30);  
    hst[i].synt->SetBarOffset(0.35);
    hst[i].intensive->SetMarkerSize(GR3MARK);  hst[i].intensive->SetBarWidth(0.30);  
    hst[i].intensive->SetBarOffset(0.65);
    hst[i].pos->LabelsDeflate();     
    hst[i].deltapos->SetMarkerSize(GR2MARK);   hst[i].deltapos->SetStats(0);
    hst[i].deltapos->GetYaxis()->SetTickLength(-0.01);
   }
  
  rb->h_ngeneral->Add(hst[0].deads);  rb->h_ngeneral->Add(hst[0].pos);  rb->h_ngeneral->Add(hst[0].healed);
  rb->h_rgeneral->Add(hst[1].deads);  rb->h_rgeneral->Add(hst[1].pos);  rb->h_rgeneral->Add(hst[1].healed);  

  for(i=0; i<9; i++)
   { 
    sprintf(str,"Sicily prv-%d",i);
    hpr[i]= new TH1I(str,"",DAYS_TO_PRINT2,0,DAYS_TO_PRINT2);
    (hpr[i])->SetBarWidth(0.10);   (hpr[i])->SetBarOffset(i*0.10+0.10);  (hpr[i])->SetStats(0);  
   }

  (hpr[0])->SetFillColor(kRed-7); (hpr[1])->SetFillColor(kOrange+1);
  (hpr[2])->SetFillColor(kYellow); (hpr[3])->SetFillColor(kSpring);
  (hpr[4])->SetFillColor(kTeal); (hpr[5])->SetFillColor(kAzure+2);
  (hpr[6])->SetFillColor(kBlue+1); (hpr[7])->SetFillColor(kViolet);
  (hpr[8])->SetFillColor(kMagenta+2); 

   //legends
  rb->legend = new TLegend(0.16,0.7,0.32,0.9);
  rb->legend->SetHeader("Casi totali (ogni giorno), di cui","C"); 			 //option "C" allows to center the header
  rb->legend->AddEntry(hst[0].healed,"Guariti e dimessi","f");
  rb->legend->AddEntry(hst[0].pos,"Attualmente positivi","f");  
  rb->legend->AddEntry(hst[0].deads,"Deceduti","f");

  rb->legendsick = new TLegend(0.12,0.7,0.45,0.9);
  rb->legendsick->SetHeader("Casi positivi:","C"); 			 //option "C" allows to center the header
  rb->legendsick->AddEntry(hst[0].intensive,"Terapia intensiva","f");
  rb->legendsick->AddEntry(hst[0].synt,"Ricovero in ospedale","f");
  rb->legendsick->AddEntry(hst[0].home,"Isolamento domiciliare","f");
  
  rb->legendprv = new TLegend(0.16,0.7,0.32,0.9);
  rb->legendprv->SetHeader("Casi nelle province Siciliane","C");
  rb->legendprv->AddEntry(hpr[0],"Trapani","f");
  rb->legendprv->AddEntry(hpr[1],"Palermo","f");
  rb->legendprv->AddEntry(hpr[2],"Messina","f");
  rb->legendprv->AddEntry(hpr[3],"Agrigento","f");
  rb->legendprv->AddEntry(hpr[4],"Caltanissetta","f");
  rb->legendprv->AddEntry(hpr[5],"Enna","f");
  rb->legendprv->AddEntry(hpr[6],"Catania","f");
  rb->legendprv->AddEntry(hpr[7],"Ragusa","f");
  rb->legendprv->AddEntry(hpr[8],"Siracusa","f");

  //opens NATIONAL file
  sprintf(str,FDIRNAME"/" F_NTL_CSV);   printf(" > opening: %s\n",str);
  if((pfn=fopen(str,"r"))==NULL) { printf("ERROR opening %s",str); return EXIT_FAILURE; } 
  //opens REGIONAL file 
  sprintf(str,FDIRNAME"/" F_REG_CSV);   printf(" > opening: %s\n",str);
  if((pfr=fopen(str,"r"))==NULL) { printf("ERROR opening %s",str); return EXIT_FAILURE; } 
  //opens PROVINCE file
  sprintf(str,FDIRNAME"/" F_PRV_CSV);   printf(" > opening: %s\n",str);
  if((pfp=fopen(str,"r"))==NULL) { printf("ERROR opening %s",str); return EXIT_FAILURE; }
  
  if(sb->cfg==0) if((pfs=fopen(CFG_FILE,"w"))==NULL) { printf("ERROR opening " CFG_FILE); return EXIT_FAILURE; }

  fscanf(pfn,"%s", str); if(sb->cfg==0) fprintf(pfs,"%s\n",str);
  fscanf(pfr,"%s", str); if(sb->cfg==0) fprintf(pfs,"%s\n",str);
  fscanf(pfp,"%s", str); if(sb->cfg==0) fprintf(pfs,"%s\n",str);
  
  if(sb->cfg==0) fclose(pfs);

  
  //READING DATAS!
  printf(" >> Beginning data elaboration...\n");
  for(i=0; i<(sb->n_days); i++)
   { 
    //NATIONAL FILE
    sprintf(daystr,"%02u-%02u",c_day.dd,c_day.mm);
    fscanf(pfn,"%s", str);    index=0;   skip_fields(&index, str, 2);
    
    if((sb->n_days)-i<=DAYS_TO_PRINT)  //for this cathegories we print only last DAYS_TO_PRINT days...
     {
      (hst[0].synt)->Fill(daystr,0);  (hst[0].intensive)->Fill(daystr,0);
      (hst[0].home)->Fill(daystr,0);
      //index PREVIOUSLY to 'ricoverati_con_sintomi'
      (hst[0].synt)->SetBinContent(i+1+DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str));
      skip_fields(&index, str, 1);     //index to 'terapia_intensiva'
      (hst[0].intensive)->SetBinContent(i+1+DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str));
      skip_fields(&index, str, 2);     //index to 'isolamento_domiciliare'
      (hst[0].home)->SetBinContent(i+1+DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str)); 
      //skip_fields(&index, str, 3);   //for debug  
     }
    else skip_fields(&index, str, 3);  //index to 'isolamento_domiciliare'

    (hst[0].deads)->Fill(daystr,0);   (hst[0].pos)->Fill(daystr,0);
    (hst[0].healed)->Fill(daystr,0);  
    skip_fields(&index, str, 1);  //index to 'totale_positivi'
    (hst[0].pos)->SetBinContent(i+1,read_int_field(&index,str));
    skip_fields(&index, str, 1);  //index to 'variazione_totale_positivi'
    if((sb->n_days)-i<=2*DAYS_TO_PRINT) 
     {
      (hst[0].deltapos)->Fill(daystr,0);
      (hst[0].deltapos)->SetBinContent(i+1+2*DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str));
     } 
    skip_fields(&index, str, 2);  //index to 'dimessi_guariti'
    (hst[0].healed)->SetBinContent(i+1,read_int_field(&index,str));
    skip_fields(&index, str, 1);  //index to 'deceduti'
    (hst[0].deads)->SetBinContent(i+1,read_int_field(&index,str)); 
    
    printf("printf\n");
    
    //REGIONAL FILE
    do { fscanf(pfr,"%s", str);   index=0;   skip_fields(&index, str, 2); } 
     while(read_int_field(&index,str)!=REGION_ID);
    skip_fields(&index, str, 4);    //index to 'ricoverati_con_sintomi'
    //printf("REGION read:? %s\n",str);

    if((sb->n_days)-i<=DAYS_TO_PRINT)  //for this cathegories we print only last DAYS_TO_PRINT days...
     {
      (hst[1].synt)->Fill(daystr,0);  (hst[1].intensive)->Fill(daystr,0);
      (hst[1].home)->Fill(daystr,0);
      //index PREVIOUSLY to 'ricoverati_con_sintomi'
      (hst[1].synt)->SetBinContent(i+1+DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str));
      skip_fields(&index, str, 1);  //index to 'terapia_intensiva'
      (hst[1].intensive)->SetBinContent(i+1+DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str));
      skip_fields(&index, str, 2);  //index to 'isolamento_domiciliare'
      (hst[1].home)->SetBinContent(i+1+DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str));  
     }
    else skip_fields(&index, str, 3);

    (hst[1].deads)->Fill(daystr,0);   (hst[1].pos)->Fill(daystr,0);
    (hst[1].healed)->Fill(daystr,0);
    skip_fields(&index, str, 1);  //index to 'totale_positivi'
    (hst[1].pos)->SetBinContent(i+1,read_int_field(&index,str));
    skip_fields(&index, str, 1);  //index to 'variazione_totale_positivi'
    if((sb->n_days)-i<=2*DAYS_TO_PRINT) 
     {
      (hst[1].deltapos)->Fill(daystr,0);
      (hst[1].deltapos)->SetBinContent(i+1+2*DAYS_TO_PRINT-(sb->n_days),read_int_field(&index,str));
     } 
    skip_fields(&index, str, 2);  //index to 'dimessi_guariti'
    (hst[1].healed)->SetBinContent(i+1,read_int_field(&index,str));
    skip_fields(&index, str, 1);  //index to 'deceduti'
    (hst[1].deads)->SetBinContent(i+1,read_int_field(&index,str));
    
    //PROVINCE FILE
    do { fscanf(pfp,"%s", str);   index=0;   skip_fields(&index, str, 2); } 
     while(read_int_field(&index,str)!=REGION_ID);
    skip_fields(&index, str, 2);  //index to 'codice_provincia'
    max_prov=0;
    for(j=0; j<9; j++)
     {
      if((sb->n_days)-i<=DAYS_TO_PRINT2)
       {
        k=read_int_field(&index,str)-PRV_FIRST_ID;
        if(k<0 || k>9) { printf(" ERROR in provinces!\n");  break; }
        skip_fields(&index, str, 5);  //index to 'totale_casi'
        
        (hpr[k])->Fill(daystr,0); temp_prov=read_int_field(&index,str);
        if(max_prov< temp_prov) max_prov=temp_prov;
        (hpr[k])->SetBinContent(i+1+DAYS_TO_PRINT2-(sb->n_days),temp_prov);
       }
      fscanf(pfp,"%s", str);  index = 0;
      skip_fields(&index, str, 4); //index to 'codice_provincia'
     }
    
    //updates html file with last day
    if(i==(sb->n_days)-1) 
     { writehtml(&c_day);  
       printf("\n\nLast day: %02d-%02d-%04d\n\n",c_day.dd,c_day.mm,c_day.yy);  }
    
    //DAY OPERATIONS:
    increment_date(&c_day); printf("iday: %02d-%02d-%04d\n",c_day.dd,c_day.mm,c_day.yy);
   }
  printf("\n  Datas have been acquired! Closing files.\n\n");
  
  fclose(pfn); fclose(pfr); fclose(pfp);
  
  (hpr[0])->SetMinimum(0); (hpr[0])->SetMaximum((int)max_prov*1.1);

  for(i=0; i<2; i++) rb->hst[i]=hst[i]; free(hst);
  for(i=0; i<9; i++) rb->hpr[i]=hpr[i]; free(hpr);
  sb->status = 2;
 }




//int main(void) //eheh mattacchione...
void covidd(void)
 {  //function executed by ROOT
  int i,j,k;        char str[STRLEN];
  setting_block sb;    sb.status = sb.cfg = 0;

  root_block rb;
  rb.hst = (exdata*)malloc(sizeof(exdata)*2);
   //from now on hst[0] is for NATIONAL set of datas, hst[1] is for SICILY set of datas
  rb.hpr = (TH1I**)malloc(sizeof(TH1I*)*9);
   //pointer to Sicily provinces histograms
  TCanvas** cvs = (TCanvas**)malloc(9*sizeof(TCanvas*));
   //pointer to charts

  //::::::::::::::::

  printf(" Welcome in " BRED "CoViD-19 " cclear "Sicily data extractor!\n");
  printf("   This program extracts from local folder CSV files\n");
  printf("   the correct datas to make some plots in root...\n");

  if(!test_files(&sb)) 
   {
    printf(" WARNING: missing database files in directory!\n");
    printf("\n You want to download new datas from " YEL "GitHub" cclear "? [y/n] > ");
    scanf("%s",str);        if(str[0]=='Y' || str[0]=='y') download();  
   }
  else sb = database_info(&sb);

  //MAIN MENU
  i=0;
  do {
   printf("\n\n >> " YEL "covidd main menu" cclear ":\n");
   if(sb.status==2)
    {
     printf("  [1] -> all single-plots for Sicily & Italy\n");
     printf("  [2] -> Sicily province plot\n");
     printf("  [3] -> overview plots for Sicily & Italy\n");
     printf("  [4] -> PLOT ALL (for website update)\n");
    }
   printf("\n  [d] -> update local database of datas\n");
   printf("  [e] -> elaborate datas in current database\n");
   printf("  [u] -> upload folder to website\n");
   printf("\n  [m] -> perform MACRO: update, elaborate & upload\n");
   printf("\n  [i] -> infos\n");
   printf("  [0] -> exit program\n");
   printf(" >> selection > ");   scanf("%s",str); 
 
   j=assign(str,"ideum","56789"); //printf("j=%d\n",j); //debug
   if(j==-1) j=atoi(str);
   if(j==0 && str[0]!='0' && str[1]!='\0') { printf("...not valid choice\n"); continue; }
   if(sb.status<2 && j<=4 && j>=1)
    { printf("you can't plot before elaborating datas!\n"); continue; }


   switch(j)
    {
     case 0: /*exit from loop*/ i=1;  break;  
     case 1:  //all single-plots for Sicily & Italy
      cvs[0] = plot_general(0, &rb);
      cvs[1] = plot_general(1, &rb);
      //saving request
      printf("You want to save canvases as png? [y/n] > ");     scanf("%s",str);      
      if(str[0]=='Y' || str[0]=='y') 
       { cvs[0]->SaveAs(ODIRNAME PDIRNAME "generalITA.png"); 
         cvs[1]->SaveAs(ODIRNAME PDIRNAME "generalSIC.png");  }
      break;
     case 2:  //Sicily province plot
      cvs[0] = plot_province(&rb);
      //saving request
      printf("You want to save canvases as png? [y/n] > ");     scanf("%s",str);      
      if(str[0]=='Y' || str[0]=='y') 
       { cvs[0]->SaveAs(ODIRNAME PDIRNAME "SICprv.png");  }
      break;
     case 3:  //overview plots for Sicily & Italy
      cvs[0] = plot_general(0, &rb);
      cvs[1] = plot_general(1, &rb);
      //saving request
      printf("You want to save canvases as png? [y/n] > ");     scanf("%s",str);      
      if(str[0]=='Y' || str[0]=='y') 
       { cvs[0]->SaveAs(ODIRNAME PDIRNAME "generalITA.png"); 
         cvs[1]->SaveAs(ODIRNAME PDIRNAME "generalSIC.png");  }
      break;
     case 4:  //PLOT ALL (for website update)
      cvs[0] = plot_general(0, &rb);       cvs[1] = plot_general(1, &rb);
      cvs[2] = plot_single_1(0, &rb);      cvs[3] = plot_single_2(0, &rb);
      cvs[4] = plot_single_3(0, &rb);      cvs[5] = plot_single_1(1, &rb);
      cvs[6] = plot_single_2(1, &rb);      cvs[7] = plot_single_3(1, &rb);
      cvs[8] = plot_province(&rb);
      //saving request
      printf("You want to save canvases as png? [y/n] > ");     scanf("%s",str);      
      if(str[0]=='Y' || str[0]=='y') 
       { cvs[0]->SaveAs(ODIRNAME PDIRNAME "generalITA.png"); cvs[1]->SaveAs(ODIRNAME PDIRNAME "generalSIC.png"); 
         cvs[2]->SaveAs(ODIRNAME PDIRNAME "ITAgen1.png"); cvs[3]->SaveAs(ODIRNAME PDIRNAME "ITAgen2.png"); 
         cvs[4]->SaveAs(ODIRNAME PDIRNAME "ITAgen3.png"); cvs[5]->SaveAs(ODIRNAME PDIRNAME "SICgen1.png");
         cvs[6]->SaveAs(ODIRNAME PDIRNAME "SICgen2.png"); cvs[7]->SaveAs(ODIRNAME PDIRNAME "SICgen3.png"); 
         cvs[8]->SaveAs(ODIRNAME PDIRNAME "SICprv.png");  }
      break;
     case 5: //infos
      printf(" made by Francesco Barone"); 
      break;
     case 6: //update local database of datas
      printf("\n You want to download new datas from " YEL "GitHub" cclear "? [y/n] > ");
      scanf("%s",str);        if(str[0]=='Y' || str[0]=='y') download();      
      break;
     case 7: //elaborate datas in current database
      sb = database_info(&sb);
      elaborate(&rb, &sb);  break;
     case 8: //upload folder to website
      system("cd online && git init && git rm --cached canvas -r && git commit -m 'rm for update' && git add . && git commit -m 'updating canvas' && git push -u origin master --force");
      break;
     case 9: //perform MACRO
      download();
      sb = database_info(&sb);
      elaborate(&rb, &sb);
      cvs[0] = plot_general(0, &rb);       cvs[1] = plot_general(1, &rb);
      cvs[2] = plot_single_1(0, &rb);      cvs[3] = plot_single_2(0, &rb);
      cvs[4] = plot_single_3(0, &rb);      cvs[5] = plot_single_1(1, &rb);
      cvs[6] = plot_single_2(1, &rb);      cvs[7] = plot_single_3(1, &rb);
      cvs[8] = plot_province(&rb);
      //saving
      cvs[0]->SaveAs(ODIRNAME PDIRNAME "generalITA.png"); cvs[1]->SaveAs(ODIRNAME PDIRNAME "generalSIC.png"); 
      cvs[2]->SaveAs(ODIRNAME PDIRNAME "ITAgen1.png");    cvs[3]->SaveAs(ODIRNAME PDIRNAME "ITAgen2.png"); 
      cvs[4]->SaveAs(ODIRNAME PDIRNAME "ITAgen3.png");    cvs[5]->SaveAs(ODIRNAME PDIRNAME "SICgen1.png");
      cvs[6]->SaveAs(ODIRNAME PDIRNAME "SICgen2.png");    cvs[7]->SaveAs(ODIRNAME PDIRNAME "SICgen3.png"); 
      cvs[8]->SaveAs(ODIRNAME PDIRNAME "SICprv.png"); 
      system("cd online && git init && git rm --cached canvas -r && git commit -m 'rm for update' && git add . && git commit -m 'updating canvas' && git push -u origin master --force");
      break;
    }

  } while(i!=1);

  printf("\n\nExit from covidd. Bye!\n\n");


  /*
  //DRAWING OPERATIONS (prv)
  //TText T;  T.SetTextAlign(21);    gStyle->SetGridStyle(0);
  //(hpr[0])->SetMinimum(0); (hpr[0])->SetMaximum((int)max_prov*1.1);
    
  rb.T.SetTextAlign(21);  gStyle->SetGridStyle(0);
  */

 }

int main(void) { covidd(); }
