//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: editinstrument.cpp,v 1.2.2.6 2009/05/31 05:12:12 terminator356 Exp $
//
//  (C) Copyright 2003 Werner Schweer (ws@seh.de)
//  (C) Copyright 2012 Tim E. Real (terminator356 on users dot sourceforge dot net)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================

#include <stdio.h> 
#include <errno.h>

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QWhatsThis>
#include <QStringListModel>
#include <QScrollBar>
#include <QVariant>
#include <QList>
#include <list>

#include "editinstrument.h"
#include "minstrument.h"
#include "globals.h"
#include "song.h"
#include "xml.h"
#include "midictrl.h"
#include "gconfig.h"
#include "icons.h"
#include "popupmenu.h"
#include "dlist.h"
#include "drummap.h"
#include "header.h"

namespace MusECore {
extern int string2sysex(const QString& s, unsigned char** data);
extern QString sysex2string(int len, unsigned char* data);
}

namespace MusEGui {

enum {
      COL_CNAME = 0, COL_TYPE,
      COL_HNUM, COL_LNUM, COL_MIN, COL_MAX, COL_DEF, COL_SHOW_MIDI, COL_SHOW_DRUM
      };

//---------------------------------------------------------
//   EditInstrument
//---------------------------------------------------------

EditInstrument::EditInstrument(QWidget* parent, Qt::WFlags fl)
   : QMainWindow(parent, fl)
      {
      setupUi(this);

      ctrlType->addItem(tr("Control7"), MusECore::MidiController::Controller7);
      ctrlType->addItem(tr("Control14"), MusECore::MidiController::Controller14);
      ctrlType->addItem(tr("RPN"), MusECore::MidiController::RPN);
      ctrlType->addItem(tr("NPRN"), MusECore::MidiController::NRPN);
      ctrlType->addItem(tr("RPN14"), MusECore::MidiController::RPN14);
      ctrlType->addItem(tr("NRPN14"), MusECore::MidiController::NRPN14);
      ctrlType->addItem(tr("Pitch"), MusECore::MidiController::Pitch);
      ctrlType->addItem(tr("Program"), MusECore::MidiController::Program);
      ctrlType->addItem(tr("PolyAftertouch"), MusECore::MidiController::PolyAftertouch);  
      ctrlType->addItem(tr("Aftertouch"), MusECore::MidiController::Aftertouch);
      ctrlType->setCurrentIndex(0);

      fileNewAction->setIcon(QIcon(*filenewIcon));
      fileOpenAction->setIcon(QIcon(*openIcon));
      fileSaveAction->setIcon(QIcon(*saveIcon));
      fileSaveAsAction->setIcon(QIcon(*saveasIcon));
      fileCloseAction->setIcon(QIcon(*exitIcon));
      viewController->setSelectionMode(QAbstractItemView::SingleSelection);
      toolBar->addAction(QWhatsThis::createAction(this));
      Help->addAction(QWhatsThis::createAction(this));

      // REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
      checkBoxGM->setEnabled(false);
      checkBoxGS->setEnabled(false);
      checkBoxXG->setEnabled(false);
      checkBoxGM->setVisible(false);
      checkBoxGS->setVisible(false);
      checkBoxXG->setVisible(false);
        
      // populate instrument list
      oldMidiInstrument = 0;
      oldPatchItem = 0;
      for (MusECore::iMidiInstrument i = MusECore::midiInstruments.begin(); i != MusECore::midiInstruments.end(); ++i) {
            // Imperfect, crude way of ignoring internal instruments, soft synths etc. If it has a gui, 
            //  it must be an internal instrument. But this will still allow some vst instruments (without a gui) 
            //  to show up in the list.
            // Hmm, try file path instead...
            //if((*i)->hasGui())
            if((*i)->filePath().isEmpty())
              continue;
              
            QListWidgetItem* item = new QListWidgetItem((*i)->iname());
            QVariant v = qVariantFromValue((void*)(*i));
            item->setData(Qt::UserRole, v);
            instrumentList->addItem(item);
            }
      instrumentList->setSelectionMode(QAbstractItemView::SingleSelection);
      if(instrumentList->item(0))
        instrumentList->setCurrentItem(instrumentList->item(0));
      
      dlist_header = new Header(dlistContainer, "header");
      dlist_header->setFixedHeight(31);
      dlist_header->setColumnLabel(tr("Name"), COL_NAME, 120);
      dlist_header->setColumnLabel(tr("Vol"), COL_VOLUME);
      dlist_header->setColumnLabel(tr("Quant"), COL_QUANT, 30);
      dlist_header->setColumnLabel(tr("E-Note"), COL_INPUTTRIGGER, 50);
      dlist_header->setColumnLabel(tr("Len"), COL_NOTELENGTH);
      dlist_header->setColumnLabel(tr("A-Note"), COL_NOTE, 50);
      dlist_header->setColumnLabel(tr("LV1"), COL_LEVEL1);
      dlist_header->setColumnLabel(tr("LV2"), COL_LEVEL2);
      dlist_header->setColumnLabel(tr("LV3"), COL_LEVEL3);
      dlist_header->setColumnLabel(tr("LV4"), COL_LEVEL4);
      dlist_header->hideSection(COL_OUTPORT);
      dlist_header->hideSection(COL_OUTCHANNEL);
      dlist_header->hideSection(COL_HIDE);
      dlist_header->hideSection(COL_MUTE);
      dlist_header->hide();
      
      ctrlValidLabel->setPixmap(*greendotIcon);
      
      connect(patchFromBox, SIGNAL(valueChanged(int)), this, SLOT(patchCollectionSpinboxChanged(int)));
      connect(patchToBox,   SIGNAL(valueChanged(int)), this, SLOT(patchCollectionSpinboxChanged(int)));
      connect(lbankFromBox, SIGNAL(valueChanged(int)), this, SLOT(patchCollectionSpinboxChanged(int)));
      connect(lbankToBox,   SIGNAL(valueChanged(int)), this, SLOT(patchCollectionSpinboxChanged(int)));
      connect(hbankFromBox, SIGNAL(valueChanged(int)), this, SLOT(patchCollectionSpinboxChanged(int)));
      connect(hbankToBox,   SIGNAL(valueChanged(int)), this, SLOT(patchCollectionSpinboxChanged(int)));
      connect(patchCheckbox, SIGNAL(toggled(bool)), this, SLOT(patchCollectionCheckboxChanged(bool)));
      connect(lbankCheckbox, SIGNAL(toggled(bool)), this, SLOT(patchCollectionCheckboxChanged(bool)));
      connect(hbankCheckbox, SIGNAL(toggled(bool)), this, SLOT(patchCollectionCheckboxChanged(bool)));
      
      connect(addCollBtn, SIGNAL(clicked()), this, SLOT(addPatchCollection()));
      connect(rmCollBtn, SIGNAL(clicked()), this, SLOT(delPatchCollection()));
      connect(copyCollBtn, SIGNAL(clicked()), this, SLOT(copyPatchCollection()));
      connect(collUpBtn, SIGNAL(clicked()), this, SLOT(patchCollectionUp()));
      connect(collDownBtn, SIGNAL(clicked()), this, SLOT(patchCollectionDown()));
      
      connect(patchCollections, SIGNAL(activated(const QModelIndex&)), this, SLOT(patchActivated(const QModelIndex&)));
      connect(patchCollections, SIGNAL(clicked  (const QModelIndex&)), this, SLOT(patchActivated(const QModelIndex&)));
      
      patch_coll_model=new QStringListModel();
      patchCollections->setModel(patch_coll_model);
      patchCollections->setEditTriggers(QAbstractItemView::NoEditTriggers);
      
      connect(instrumentList, SIGNAL(itemSelectionChanged()), SLOT(instrumentChanged()));
      connect(patchView, SIGNAL(itemSelectionChanged()), SLOT(patchChanged()));

      dlist_vscroll = new QScrollBar(Qt::Vertical, this);
      dlist_vscroll->setMaximum(128*TH);
      dlist_vscroll->hide();
      
      dlist_grid = new QGridLayout(dlistContainer);
      dlist_grid->setContentsMargins(0, 0, 0, 0);
      dlist_grid->setSpacing(0);  
      dlist_grid->setRowStretch(1, 100);
      dlist_grid->setColumnStretch(0, 100);
      dlist_grid->addWidget(dlist_header, 0, 0);
      dlist_grid->addWidget(dlist_vscroll, 1,1);
      
      dlist=NULL;

      changeInstrument();
      
      connect(viewController, SIGNAL(itemSelectionChanged()), SLOT(controllerChanged()));
      
      connect(instrumentName, SIGNAL(returnPressed()), SLOT(instrumentNameReturn()));
      connect(instrumentName, SIGNAL(lostFocus()), SLOT(instrumentNameReturn()));
      
      connect(patchNameEdit, SIGNAL(returnPressed()), SLOT(patchNameReturn()));
      connect(patchNameEdit, SIGNAL(lostFocus()), SLOT(patchNameReturn()));
      connect(patchDelete, SIGNAL(clicked()), SLOT(deletePatchClicked()));
      connect(patchNew, SIGNAL(clicked()), SLOT(newPatchClicked()));
      connect(patchNewGroup, SIGNAL(clicked()), SLOT(newGroupClicked()));
      
      connect(patchButton, SIGNAL(clicked()), SLOT(patchButtonClicked()));
      connect(defPatchH, SIGNAL(valueChanged(int)), SLOT(defPatchChanged(int)));
      connect(defPatchL, SIGNAL(valueChanged(int)), SLOT(defPatchChanged(int)));
      connect(defPatchProg, SIGNAL(valueChanged(int)), SLOT(defPatchChanged(int)));
      connect(deleteController, SIGNAL(clicked()), SLOT(deleteControllerClicked()));
      connect(newController, SIGNAL(clicked()), SLOT(newControllerClicked()));
      connect(addController, SIGNAL(clicked()), SLOT(addControllerClicked()));
      connect(ctrlType,SIGNAL(activated(int)), SLOT(ctrlTypeChanged(int)));
      connect(ctrlName, SIGNAL(returnPressed()), SLOT(ctrlNameReturn()));
      connect(ctrlName, SIGNAL(lostFocus()), SLOT(ctrlNameReturn()));
      connect(spinBoxHCtrlNo, SIGNAL(valueChanged(int)), SLOT(ctrlNumChanged()));
      connect(spinBoxLCtrlNo, SIGNAL(valueChanged(int)), SLOT(ctrlNumChanged()));
      connect(spinBoxMin, SIGNAL(valueChanged(int)), SLOT(ctrlMinChanged(int)));
      connect(spinBoxMax, SIGNAL(valueChanged(int)), SLOT(ctrlMaxChanged(int)));
      connect(spinBoxDefault, SIGNAL(valueChanged(int)), SLOT(ctrlDefaultChanged(int)));
      connect(nullParamSpinBoxH, SIGNAL(valueChanged(int)), SLOT(ctrlNullParamHChanged(int)));
      connect(nullParamSpinBoxL, SIGNAL(valueChanged(int)), SLOT(ctrlNullParamLChanged(int)));
      connect(ctrlShowInMidi,SIGNAL(stateChanged(int)), SLOT(ctrlShowInMidiChanged(int)));
      connect(ctrlShowInDrum,SIGNAL(stateChanged(int)), SLOT(ctrlShowInDrumChanged(int)));
      
      connect(tabWidget3, SIGNAL(currentChanged(QWidget*)), SLOT(tabChanged(QWidget*)));
      connect(sysexList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
         SLOT(sysexChanged(QListWidgetItem*, QListWidgetItem*)));
      connect(deleteSysex, SIGNAL(clicked()), SLOT(deleteSysexClicked()));
      connect(newSysex, SIGNAL(clicked()), SLOT(newSysexClicked()));
      }

void EditInstrument::findInstrument(const QString& find_instrument)
{
  if(find_instrument.isEmpty())
    return;
  QList<QListWidgetItem*> found = instrumentList->findItems(find_instrument, Qt::MatchExactly);
  if(!found.isEmpty())
    instrumentList->setCurrentItem(found.at(0));
}

void EditInstrument::showTab(TabType n)
{
  if(n >= tabWidget3->count())
    return;
  tabWidget3->setCurrentIndex(n);
}

      
void EditInstrument::patchCollectionSpinboxChanged(int)
{
  if (patchFromBox->value() > patchToBox->value())
    patchToBox->setValue(patchFromBox->value());

  if (lbankFromBox->value() > lbankToBox->value())
    lbankToBox->setValue(lbankFromBox->value());

  if (hbankFromBox->value() > hbankToBox->value())
    hbankToBox->setValue(hbankFromBox->value());
  
  storePatchCollection();
}

void EditInstrument::patchCollectionCheckboxChanged(bool)
{
  storePatchCollection();
}

void EditInstrument::storePatchCollection()
{
  using MusECore::patch_drummap_mapping_t;
  
  int idx=patchCollections->currentIndex().row();
  std::list<patch_drummap_mapping_t>* pdm = workingInstrument.get_patch_drummap_mapping();
  if (idx>=0 && (unsigned)idx<pdm->size())
  {
    std::list<patch_drummap_mapping_t>::iterator it=pdm->begin();
    advance(it,idx);
    
    if (patchCheckbox->isChecked())
    {
      it->affected_patches.first_program=patchFromBox->value()-1;
      it->affected_patches.last_program=patchToBox->value()-1;
    }
    else
    {
      it->affected_patches.first_program=0;
      it->affected_patches.last_program=127;
    }

    if (lbankCheckbox->isChecked())
    {
      it->affected_patches.first_lbank=lbankFromBox->value()-1;
      it->affected_patches.last_lbank=lbankToBox->value()-1;
    }
    else
    {
      it->affected_patches.first_lbank=0;
      it->affected_patches.last_lbank=127;
    }

    if (hbankCheckbox->isChecked())
    {
      it->affected_patches.first_hbank=hbankFromBox->value()-1;
      it->affected_patches.last_hbank=hbankToBox->value()-1;
    }
    else
    {
      it->affected_patches.first_hbank=0;
      it->affected_patches.last_hbank=127;
    }
    
    workingInstrument.setDirty(true);
    repopulatePatchCollections();
  }
}

void EditInstrument::fetchPatchCollection()
{
  using MusECore::patch_drummap_mapping_t;
  
  int idx=patchCollections->currentIndex().row();
  std::list<patch_drummap_mapping_t>* pdm = workingInstrument.get_patch_drummap_mapping();
  if (idx>=0 && (unsigned)idx<pdm->size())
  {
    std::list<patch_drummap_mapping_t>::iterator it=pdm->begin();
    advance(it,idx);
    
    patchFromBox->blockSignals(true);
    patchToBox->blockSignals(true);
    lbankFromBox->blockSignals(true);
    lbankToBox->blockSignals(true);
    hbankFromBox->blockSignals(true);
    hbankToBox->blockSignals(true);
    
    patchFromBox->setValue(it->affected_patches.first_program+1);
    patchToBox->setValue(it->affected_patches.last_program+1);
    
    lbankFromBox->setValue(it->affected_patches.first_lbank+1);
    lbankToBox->setValue(it->affected_patches.last_lbank+1);
    
    hbankFromBox->setValue(it->affected_patches.first_hbank+1);
    hbankToBox->setValue(it->affected_patches.last_hbank+1);

    patchFromBox->blockSignals(false);
    patchToBox->blockSignals(false);
    lbankFromBox->blockSignals(false);
    lbankToBox->blockSignals(false);
    hbankFromBox->blockSignals(false);
    hbankToBox->blockSignals(false);


    patchCheckbox->setChecked(it->affected_patches.first_program>0 || it->affected_patches.last_program < 127);
    lbankCheckbox->setChecked(it->affected_patches.first_lbank>0 || it->affected_patches.last_lbank < 127);
    hbankCheckbox->setChecked(it->affected_patches.first_hbank>0 || it->affected_patches.last_hbank < 127);
  }
}

void EditInstrument::patchActivated(const QModelIndex& idx)
{
  using MusECore::patch_drummap_mapping_t;
  
  if (idx.row()>=0)
  {
    using MusECore::DrumMap;
    
    std::list<patch_drummap_mapping_t>* tmp = workingInstrument.get_patch_drummap_mapping();
    std::list<patch_drummap_mapping_t>::iterator it=tmp->begin();
    if ((unsigned)idx.row()>=tmp->size())
      printf("THIS SHOULD NEVER HAPPEN: idx.row()>=tmp->size() in EditInstrument::patchActivated()\n");
    
    advance(it, idx.row());
    DrumMap* dm=it->drummap;
    
    
    if (dlist)
    {
      dlist->hide();
      delete dlist;
      dlist=NULL;
    }
    
    dlist=new DList(dlist_header,dlistContainer,1,dm);
    
    dlist->setYPos(dlist_vscroll->value());
    
    connect(dlist_vscroll, SIGNAL(valueChanged(int)), dlist, SLOT(setYPos(int)));
    dlist_grid->addWidget(dlist, 1, 0);


    dlist_header->show();
    dlist->show();
    dlist_vscroll->show();
    
    collUpBtn->setEnabled(idx.row()>0);
    collDownBtn->setEnabled(idx.row()<patch_coll_model->rowCount()-1);
    rmCollBtn->setEnabled(true);
    copyCollBtn->setEnabled(true);
    patchCollectionContainer->setEnabled(true);

    fetchPatchCollection();
  }
}

void EditInstrument::addPatchCollection()
{
  using MusECore::patch_drummap_mapping_t;
  
  int idx=patchCollections->currentIndex().row();
  
  std::list<patch_drummap_mapping_t>* tmp = workingInstrument.get_patch_drummap_mapping();
  std::list<patch_drummap_mapping_t>::iterator it=tmp->begin();
  advance(it,idx+1);
  tmp->insert(it,patch_drummap_mapping_t());

  repopulatePatchCollections();
  patchCollections->setCurrentIndex(patch_coll_model->index(idx+1));
  patchActivated(patchCollections->currentIndex());
  
  workingInstrument.setDirty(true);
}

void EditInstrument::delPatchCollection()
{
  using MusECore::patch_drummap_mapping_t;
  
  int idx=patchCollections->currentIndex().row();
  if (idx>=0)
  {
    if (dlist)
    {
      dlist->hide();
      delete dlist;
      dlist=NULL;
    }
    
    dlist_header->hide();
    dlist_vscroll->hide();

    rmCollBtn->setEnabled(false);
    copyCollBtn->setEnabled(false);
    patchCollectionContainer->setEnabled(false);
    collUpBtn->setEnabled(false);
    collDownBtn->setEnabled(false);

    std::list<patch_drummap_mapping_t>* tmp = workingInstrument.get_patch_drummap_mapping();
    std::list<patch_drummap_mapping_t>::iterator it=tmp->begin();
    advance(it,idx);
    tmp->erase(it);

    repopulatePatchCollections();
    
    patchActivated(patchCollections->currentIndex());
    workingInstrument.setDirty(true);
  }
}

void EditInstrument::copyPatchCollection()
{
  using MusECore::patch_drummap_mapping_t;
  
  int idx=patchCollections->currentIndex().row();
  
  std::list<patch_drummap_mapping_t>* tmp = workingInstrument.get_patch_drummap_mapping();
  std::list<patch_drummap_mapping_t>::iterator it=tmp->begin();
  advance(it,idx);
  patch_drummap_mapping_t tmp2(*it);
  it++;
  tmp->insert(it,tmp2);
  
  patch_coll_model->insertRow(idx+1);
  patch_coll_model->setData(patch_coll_model->index(idx+1), patch_coll_model->index(idx).data());
  patchCollections->setCurrentIndex(patch_coll_model->index(idx+1));
  patchActivated(patchCollections->currentIndex());
  workingInstrument.setDirty(true);
}

void EditInstrument::patchCollectionUp()
{
  using MusECore::patch_drummap_mapping_t;
  
  std::list<patch_drummap_mapping_t>* pdm = workingInstrument.get_patch_drummap_mapping();
  int idx=patchCollections->currentIndex().row();
  
  if (idx>=1)
  {
    std::list<patch_drummap_mapping_t>::iterator it=pdm->begin();
    advance(it,idx-1);
    std::list<patch_drummap_mapping_t>::iterator it2=it;
    it2++;
    
    //it2 is the element to move, it is the element to put before.
    
    pdm->insert(it,*it2);
    pdm->erase(it2);
    
    repopulatePatchCollections();

    patchCollections->setCurrentIndex(patch_coll_model->index(idx-1));
    patchActivated(patchCollections->currentIndex());

    workingInstrument.setDirty(true);
  }
}

void EditInstrument::patchCollectionDown()
{
  using MusECore::patch_drummap_mapping_t;
  
  std::list<patch_drummap_mapping_t>* pdm = workingInstrument.get_patch_drummap_mapping();
  int idx=patchCollections->currentIndex().row();
  
  if ((unsigned)idx<pdm->size()-1)
  {
    std::list<patch_drummap_mapping_t>::iterator it=pdm->begin();
    advance(it,idx);
    std::list<patch_drummap_mapping_t>::iterator it2=it;
    it2++; it2++;
    
    //it is the element to move, it2 is the element to put before (might be end())
    
    pdm->insert(it2,*it);
    pdm->erase(it);
    
    repopulatePatchCollections();

    patchCollections->setCurrentIndex(patch_coll_model->index(idx+1));
    patchActivated(patchCollections->currentIndex());

    workingInstrument.setDirty(true);
  }
}

void EditInstrument::repopulatePatchCollections()
{
  using MusECore::patch_drummap_mapping_t;
  
  int idx=patchCollections->currentIndex().row();
  QStringList strlist;
  
  std::list<patch_drummap_mapping_t>* pdm = workingInstrument.get_patch_drummap_mapping();
  for (std::list<patch_drummap_mapping_t>::iterator it=pdm->begin(); it!=pdm->end(); it++)
    strlist << it->affected_patches.to_string();
  
  patch_coll_model->setStringList(strlist);
  patchCollections->setCurrentIndex(patch_coll_model->index(idx));
}

//---------------------------------------------------------
//   helpWhatsThis
//---------------------------------------------------------

void EditInstrument::helpWhatsThis()
{
  whatsThis();
}

//---------------------------------------------------------
//   fileNew
//---------------------------------------------------------

void EditInstrument::fileNew()
      {
      // Allow these to update...
      instrumentNameReturn();
      patchNameReturn();
      ctrlNameReturn();
      
      for (int i = 1;; ++i) {
            QString s = QString("Instrument-%1").arg(i);
            bool found = false;
            for (MusECore::iMidiInstrument i = MusECore::midiInstruments.begin(); i != MusECore::midiInstruments.end(); ++i) {
                  if (s == (*i)->iname()) {
                        found = true;
                        break;
                        }
                  }
            if (!found) {
                        MusECore::MidiInstrument* oi = 0;
                        if(oldMidiInstrument)
                          oi = (MusECore::MidiInstrument*)oldMidiInstrument->data(Qt::UserRole).value<void*>();
                        MusECore::MidiInstrument* wip = &workingInstrument;
                        if(checkDirty(wip)) // No save was chosen. Restore the actual instrument name.
                        {
                          if(oi)
                          {
                            oldMidiInstrument->setText(oi->iname());
                            
                            // No file path? Only a new unsaved instrument can do that. So delete it.
                            if(oi->filePath().isEmpty())
                              // Delete the list item and the instrument.
                              deleteInstrument(oldMidiInstrument);
                            
                          }  
                        }
                        workingInstrument.setDirty(false);
                        
                  MusECore::MidiInstrument* ni = new MusECore::MidiInstrument(s);
                  MusECore::midiInstruments.push_back(ni);
                  QListWidgetItem* item = new QListWidgetItem(ni->iname());
                  
                  workingInstrument.assign( *ni );

                  QVariant v = qVariantFromValue((void*)(ni));
                  item->setData(Qt::UserRole, v);
                  instrumentList->addItem(item);
                  
                  oldMidiInstrument = 0;
                  
                  instrumentList->blockSignals(true);
                  instrumentList->setCurrentItem(item);
                  instrumentList->blockSignals(false);
                  
                  changeInstrument();
                  
                  // We have our new instrument! So set dirty to true.
                  workingInstrument.setDirty(true);
                  
                  break;
                  }
            }

      }

//---------------------------------------------------------
//   fileOpen
//---------------------------------------------------------

void EditInstrument::fileOpen() //DELETETHIS?
      {
      }

//---------------------------------------------------------
//   fileSave
//---------------------------------------------------------

void EditInstrument::fileSave()
{
      if (workingInstrument.filePath().isEmpty())
      {
        saveAs();
        return;
      }      
      
      // Do not allow a direct overwrite of a 'built-in' muse instrument.
      QFileInfo qfi(workingInstrument.filePath());
      if(qfi.absolutePath() == MusEGlobal::museInstruments)
      {
        saveAs();
        return;
      }
      
      FILE* f = fopen(workingInstrument.filePath().toLatin1().constData(), "w");
      if(f == 0)
      {
        saveAs();
        return;
      }  
      
      // Allow these to update...
      instrumentNameReturn();
      patchNameReturn();
      ctrlNameReturn();
      
      if(fclose(f) != 0)
      {
        QString s = QString("Creating file:\n") + workingInstrument.filePath() + QString("\nfailed: ")
          + QString(strerror(errno) );
        QMessageBox::critical(this, tr("MusE: Create file failed"), s);
        return;
      }
      
      if(fileSave(&workingInstrument, workingInstrument.filePath()))
        workingInstrument.setDirty(false);
}

//---------------------------------------------------------
//   fileSave
//---------------------------------------------------------

bool EditInstrument::fileSave(MusECore::MidiInstrument* instrument, const QString& name)
    {
      FILE* f = fopen(name.toAscii().constData(), "w");
      if(f == 0)
      {
        QString s("Creating file failed: ");
        s += QString(strerror(errno));
        QMessageBox::critical(this,
            tr("MusE: Create file failed"), s);
        return false;
      }
            
      MusECore::Xml xml(f);
      
      updateInstrument(instrument);
      
      instrument->write(0, xml);
      
      // Assign the working instrument values to the actual current selected instrument...
      if(oldMidiInstrument)
      {
        MusECore::MidiInstrument* oi = (MusECore::MidiInstrument*)oldMidiInstrument->data(Qt::UserRole).value<void*>();
        if(oi)
        {
          oi->assign(workingInstrument);
          
          // Now signal the rest of the app so stuff can change...
          MusEGlobal::song->update(SC_CONFIG | SC_MIDI_CONTROLLER);
        }  
      }
      
      if(fclose(f) != 0)
      {
            QString s = QString("Write File\n") + name + QString("\nfailed: ")
               + QString(strerror(errno));
            QMessageBox::critical(this, tr("MusE: Write File failed"), s);
            return false;
      }
      return true;
    }

//---------------------------------------------------------
//   saveAs
//---------------------------------------------------------

void EditInstrument::saveAs()
    {
      // Allow these to update...
      instrumentNameReturn();
      patchNameReturn();
      ctrlNameReturn();
      
      QString path = MusEGlobal::museUserInstruments;
      if(!QDir(MusEGlobal::museUserInstruments).exists())
      {
        printf("MusE Error! User instrument directory: %s does not exist. Should be created at startup!\n", MusEGlobal::museUserInstruments.toLatin1().constData());
        
        //path = MusEGlobal::museUser; DELETETHIS?
        //path = MusEGlobal::configPath;  
      }
        
      if (workingInstrument.filePath().isEmpty())
            path += QString("/%1.idf").arg(workingInstrument.iname());
      else {
            QFileInfo fi(workingInstrument.filePath());
            
            // Prompt for a new instrument name if the name has not been changed, to avoid duplicates.
            if(oldMidiInstrument)
            {
              MusECore::MidiInstrument* oi = (MusECore::MidiInstrument*)oldMidiInstrument->data(Qt::UserRole).value<void*>();
              if(oi)
              {
                if(oi->iname() == workingInstrument.iname())
                {
                  // Prompt only if it's a user instrument, to avoid duplicates in the user instrument dir.
                  // This will still allow a user instrument to override a built-in instrument with the same name.
                  if(fi.absolutePath() != MusEGlobal::museInstruments)
                  {
                    printf("EditInstrument::saveAs Error: Instrument name %s already used!\n", workingInstrument.iname().toLatin1().constData());
                    return;    
                  }  
                }
              }
            }  
            path += QString("/%1.idf").arg(fi.baseName());
           }

      QString s = QFileDialog::getSaveFileName(this, tr("MusE: Save Instrument Definition").toLatin1().constData(), 
         path, tr("Instrument Definition (*.idf)"));
      if (s.isEmpty())
            return;
      workingInstrument.setFilePath(s);
      
      if(fileSave(&workingInstrument, s))
        workingInstrument.setDirty(false);
    }

//---------------------------------------------------------
//   fileSaveAs
//---------------------------------------------------------

void EditInstrument::fileSaveAs()
    {
      // Is this a new unsaved instrument? Just do a normal save.
      if(workingInstrument.filePath().isEmpty())
      {
        saveAs();
        return;
      }      
      
      // Allow these to update...
      instrumentNameReturn();
      patchNameReturn();
      ctrlNameReturn();
      
      MusECore::MidiInstrument* oi = 0;
      if(oldMidiInstrument)
        oi = (MusECore::MidiInstrument*)oldMidiInstrument->data(Qt::UserRole).value<void*>();
        
      int res = checkDirty(&workingInstrument, true);
      switch(res)
      {
        // No save:
        case 1:
          workingInstrument.setDirty(false);
          if(oi)
          {
            oldMidiInstrument->setText(oi->iname());
            
            // No file path? Only a new unsaved instrument can do that. So delete it.
            if(oi->filePath().isEmpty())
            {
              // Delete the list item and the instrument.
              deleteInstrument(oldMidiInstrument);
              oldMidiInstrument = 0;
            }
            
            changeInstrument();
            
          }
          return;
        break;
        
        // Abort:
        case 2: 
          return;
        break;
          
        // Save:
        case 0:
            workingInstrument.setDirty(false);
        break;
      }
      
      bool isuser = false;
      QString so;
      if(workingInstrument.iname().isEmpty())
        so = QString("Instrument");
      else  
        so = workingInstrument.iname();
        
      for(int i = 1;; ++i) 
      {
        QString s = so + QString("-%1").arg(i);
        
        bool found = false;
        for(MusECore::iMidiInstrument imi = MusECore::midiInstruments.begin(); imi != MusECore::midiInstruments.end(); ++imi) 
        {
          if(s == (*imi)->iname()) 
          {
            // Allow override of built-in instrument names.
            QFileInfo fi((*imi)->filePath());
            if(fi.absolutePath() == MusEGlobal::museInstruments)
              break;
            found = true;
            break;
          }
        }
        if(found) 
          continue;  
        
        bool ok;
        s = QInputDialog::getText(this, tr("MusE: Save instrument as"), tr("Enter a new unique instrument name:"), 
                                  QLineEdit::Normal, s, &ok);
        if(!ok) 
          return;
        if(s.isEmpty())
        {
          --i;
          continue;  
        }  
          
        isuser = false;
        bool builtin = false;
        found = false;
        for(MusECore::iMidiInstrument imi = MusECore::midiInstruments.begin(); imi != MusECore::midiInstruments.end(); ++imi) 
        {
          // If an instrument with the same name is found...
          if((*imi)->iname() == s) 
          {
            // If it's not the same name as the working instrument, and it's not an internal instrument (soft synth etc.)...
            if(s != workingInstrument.iname() && !(*imi)->filePath().isEmpty())
            {
              QFileInfo fi((*imi)->filePath());
              // Allow override of built-in and user instruments:
              // If it's a user instrument, not a built-in instrument...
              if(fi.absolutePath() == MusEGlobal::museUserInstruments)
              {
                // No choice really but to overwrite the destination instrument file!
                // Can not have two user files containing the same instrument name.
                if(QMessageBox::question(this,
                    tr("MusE: Save instrument as"),
                    tr("The user instrument '%1' already exists. This will overwrite its .idf instrument file.\nAre you sure?").arg(s),
                    QMessageBox::Ok | QMessageBox::Default,
                    QMessageBox::Cancel | QMessageBox::Escape,
                    Qt::NoButton) == QMessageBox::Ok)
                {
                  // Set the working instrument's file path to the found instrument's path.
                  workingInstrument.setFilePath((*imi)->filePath());
                  // Mark as overwriting a user instrument.
                  isuser = true;
                }  
                else
                {
                  found = true;
                  break;
                }
              }

              // Assign the found instrument name to the working instrument name.
              workingInstrument.setIName(s);
              
              // Find the instrument in the list and set the old instrument to the item.
              oldMidiInstrument = instrumentList->findItems(s, Qt::MatchExactly)[0];
              
              // Mark as a built-in instrument.
              builtin = true;
              break;
            }  
            found = true;
            break;
          }
        }
        if(found)
        { 
          so = s;
          i = 0;
          continue;  
        }  
        
        so = s;
        
        // If the name does not belong to a built-in instrument...
        if(!builtin)
        {
          MusECore::MidiInstrument* ni = new MusECore::MidiInstrument();
          ni->assign(workingInstrument);
          ni->setIName(so);
          ni->setFilePath(QString());
          MusECore::midiInstruments.push_back(ni);
          QListWidgetItem* item = new QListWidgetItem(so);
          
          workingInstrument.assign( *ni );

          QVariant v = qVariantFromValue((void*)(ni));
          item->setData(Qt::UserRole, v);
          instrumentList->addItem(item);
          
          oldMidiInstrument = 0;
          
          instrumentList->blockSignals(true);
          instrumentList->setCurrentItem(item);
          instrumentList->blockSignals(false);
          
          changeInstrument();
          
          // We have our new instrument! So set dirty to true.
          workingInstrument.setDirty(true);
        }  
          
        break;
      }
      
      QString path = MusEGlobal::museUserInstruments;
      
      if(!QDir(MusEGlobal::museUserInstruments).exists())
      {
        printf("MusE Error! User instrument directory: %s does not exist. Should be created at startup!\n", MusEGlobal::museUserInstruments.toLatin1().constData());
        
        //path = MusEGlobal::museUser; DELETETHIS
        //path = MusEGlobal::configPath;  
      }
      path += QString("/%1.idf").arg(so);
            
      QString sfn;
      // If we are overwriting a user instrument just force the path.
      if(isuser)
        sfn = path;
      else  
      {
        sfn = QFileDialog::getSaveFileName(this, tr("MusE: Save Instrument Definition").toLatin1().constData(),
           path, tr("Instrument Definition (*.idf)"));
        if (sfn.isEmpty())
              return;
        workingInstrument.setFilePath(sfn);
      }  
      
      if(fileSave(&workingInstrument, sfn))
        workingInstrument.setDirty(false);
    }

//---------------------------------------------------------
//   fileClose
//---------------------------------------------------------

void EditInstrument::fileClose()
      {
        close();
      }

//---------------------------------------------------------
//   closeEvent
//---------------------------------------------------------

void EditInstrument::closeEvent(QCloseEvent* ev)
      {
      // Allow these to update...
      instrumentNameReturn();
      patchNameReturn();
      ctrlNameReturn();
      
        MusECore::MidiInstrument* oi = 0;
        if(oldMidiInstrument)
          oi = (MusECore::MidiInstrument*)oldMidiInstrument->data(Qt::UserRole).value<void*>();
          
        int res = checkDirty(&workingInstrument, true);
        switch(res)
        {
          // No save:
          case 1:
            workingInstrument.setDirty(false);
            if(oi)
            {
              oldMidiInstrument->setText(oi->iname());
              
              // No file path? Only a new unsaved instrument can do that. So delete it.
              if(oi->filePath().isEmpty())
              {
                // Delete the list item and the instrument.
                deleteInstrument(oldMidiInstrument);
                oldMidiInstrument = 0;
              }
              
              changeInstrument();
              
            }  
          break;
          
          // Abort:
          case 2: 
            ev->ignore();
            return;
          break;
            
          // Save:
          case 0:
              workingInstrument.setDirty(false);
          break;
          
        }
      
      QMainWindow::closeEvent(ev);
      }

//---------------------------------------------------------
//   changeInstrument
//---------------------------------------------------------

void EditInstrument::changeInstrument()
{
  QListWidgetItem* sel = instrumentList->currentItem();

  if(!sel)
    return;

  oldMidiInstrument = sel;
  // Assignment
  
  // Assign will 'delete' any existing patches, groups, or controllers.
  workingInstrument.assign( *((MusECore::MidiInstrument*)sel->data(Qt::UserRole).value<void*>()) );
  
  workingInstrument.setDirty(false);
  
  // populate patch list
  patchView->blockSignals(true);
  for (int i = 0; i < patchView->topLevelItemCount(); ++i)
    qDeleteAll(patchView->topLevelItem(i)->takeChildren());
  patchView->clear();
  patchView->blockSignals(false);

  for (int i = 0; i < viewController->topLevelItemCount(); ++i)
    qDeleteAll(viewController->topLevelItem(i)->takeChildren());
  viewController->clear();

  instrumentName->blockSignals(true);
  instrumentName->setText(workingInstrument.iname());
  instrumentName->blockSignals(false);
  
  nullParamSpinBoxH->blockSignals(true);
  nullParamSpinBoxL->blockSignals(true);
  int nv = workingInstrument.nullSendValue();
  if(nv == -1)
  {
    nullParamSpinBoxH->setValue(-1);
    nullParamSpinBoxL->setValue(-1);
  }  
  else
  {
    int nvh = (nv >> 8) & 0xff;
    int nvl = nv & 0xff;
    if(nvh == 0xff)
      nullParamSpinBoxH->setValue(-1);
    else  
      nullParamSpinBoxH->setValue(nvh & 0x7f);
    if(nvl == 0xff)
      nullParamSpinBoxL->setValue(-1);
    else  
      nullParamSpinBoxL->setValue(nvl & 0x7f);
  }
  nullParamSpinBoxH->blockSignals(false);
  nullParamSpinBoxL->blockSignals(false);
  
  sysexList->blockSignals(true);
  sysexList->clear();
  foreach(const MusECore::SysEx* s, workingInstrument.sysex()) {
        QListWidgetItem* item = new QListWidgetItem(s->name);
        QVariant v = QVariant::fromValue((void*)s);
        item->setData(Qt::UserRole, v);
        sysexList->addItem(item);
        }
  if(sysexList->item(0))
    sysexList->item(0)->setSelected(true);
  sysexList->blockSignals(false);
  sysexChanged(sysexList->item(0), 0);
  
  MusECore::PatchGroupList* pg = workingInstrument.groups();
  for (MusECore::ciPatchGroup g = pg->begin(); g != pg->end(); ++g) {
        MusECore::PatchGroup* pgp = *g; 
        if(pgp)
        {
          QTreeWidgetItem* item = new QTreeWidgetItem(patchView);
          
          item->setText(0, pgp->name);
          QVariant v = qVariantFromValue((void*)(pgp));
          item->setData(0, Qt::UserRole, v);
          
          for (MusECore::ciPatch p = pgp->patches.begin(); p != pgp->patches.end(); ++p) 
          {
            MusECore::Patch* patch = *p;
            if(patch)
            {
              QTreeWidgetItem* sitem = new QTreeWidgetItem(item);
              sitem->setText(0, patch->name);
              QVariant v = QVariant::fromValue((void*)patch);
              sitem->setData(0, Qt::UserRole, v);
            }  
          }  
        }
      }  
  
  oldPatchItem = 0;
  
  QTreeWidgetItem* fc = patchView->topLevelItem(0);
  if(fc)
  {
    // This may cause a patchChanged call.
    patchView->blockSignals(true);
    fc->setSelected(true);
    patchView->blockSignals(false);
  }
      
  patchChanged();
  
  MusECore::MidiControllerList* cl = workingInstrument.controller();
  for (MusECore::ciMidiController ic = cl->begin(); ic != cl->end(); ++ic) {
        MusECore::MidiController* c = ic->second;
        addControllerToView(c);
        }
  
  QTreeWidgetItem *ci = viewController->topLevelItem(0);

  if(ci)
  {
    viewController->blockSignals(true);
    ci->setSelected(true);
    viewController->blockSignals(false);
  }  
  
  controllerChanged();
  
  repopulatePatchCollections();
  if (dlist)
  {
    dlist->hide();
    delete dlist;
    dlist=NULL;
  }
  
  dlist_header->hide();
  dlist_vscroll->hide();

  rmCollBtn->setEnabled(false);
  copyCollBtn->setEnabled(false);
  patchCollectionContainer->setEnabled(false);
  collUpBtn->setEnabled(false);
  collDownBtn->setEnabled(false);


}

//---------------------------------------------------------
//   instrumentChanged
//---------------------------------------------------------

void EditInstrument::instrumentChanged()
      {
      QListWidgetItem* sel = instrumentList->currentItem();

      if(!sel)
        return;
           
        MusECore::MidiInstrument* oi = 0;
        if(oldMidiInstrument)
          oi = (MusECore::MidiInstrument*)oldMidiInstrument->data(Qt::UserRole).value<void*>();
        MusECore::MidiInstrument* wip = &workingInstrument;
        // Returns true if aborted.
        if(checkDirty(wip))
        {
          // No save was chosen. Abandon changes, or delete if it is new...
          if(oi)
          {
            oldMidiInstrument->setText(oi->iname());
            
            // No file path? Only a new unsaved instrument can do that. So delete it.
            if(oi->filePath().isEmpty())
            {
              // Delete the list item and the instrument.
              deleteInstrument(oldMidiInstrument);
              oldMidiInstrument = 0;
            }
            
          }  
        }
        workingInstrument.setDirty(false);

        changeInstrument();
      }

//---------------------------------------------------------
//   instrumentNameReturn
//---------------------------------------------------------

void EditInstrument::instrumentNameReturn()
{
  QListWidgetItem* item = instrumentList->currentItem();

  if (item == 0)
        return;
  QString s = instrumentName->text();
  
  if(s == item->text()) 
    return;
  
  MusECore::MidiInstrument* curins = (MusECore::MidiInstrument*)item->data(Qt::UserRole).value<void*>();
  
  for(MusECore::iMidiInstrument i = MusECore::midiInstruments.begin(); i != MusECore::midiInstruments.end(); ++i) 
  {
    if((*i) != curins && s == (*i)->iname()) 
    {
      instrumentName->blockSignals(true);
      // Grab the last valid name from the item text, since the instrument has not been updated yet.
      instrumentName->setText(item->text());
      instrumentName->blockSignals(false);
      
      QMessageBox::critical(this,
          tr("MusE: Bad instrument name"),
          tr("Please choose a unique instrument name.\n(The name might be used by a hidden instrument.)"),
          QMessageBox::Ok,
          Qt::NoButton,
          Qt::NoButton);
          
      return;
    }
  }      
  
  item->setText(s);
  workingInstrument.setIName(s);
  workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   deleteInstrument
//---------------------------------------------------------

void EditInstrument::deleteInstrument(QListWidgetItem* item)
{
  if(!item)
    return;

  MusECore::MidiInstrument* ins = (MusECore::MidiInstrument*)item->data(Qt::UserRole).value<void*>();
  
  // Delete the list item.
  // Test this: Is this going to change the current selection?
  instrumentList->blockSignals(true);
  delete item;
  instrumentList->blockSignals(false);
  
  // Test this: Neccessary? DELETETHIS
  // if(curritem)
  //  instrumentList->setCurrentItem(curritem);
  
  if(!ins)
    return;
        
  // Remove the instrument from the list.
  MusECore::midiInstruments.remove(ins);
  
  // Delete the instrument.
  delete ins;
}

//---------------------------------------------------------
//   tabChanged
//   Added so that patch list is updated when switching tabs, 
//    so that 'Program' default values and text are current in controller tab. 
//---------------------------------------------------------

void EditInstrument::tabChanged(QWidget* w)
{
  if(!w)
    return;
    
  // If we're switching to the Patches tab, just ignore.
  if(QString(w->objectName()) == QString("patchesTab"))
    return;
    
  if(oldPatchItem)
  {
    // Don't bother calling patchChanged, just update the patch or group.
    if(oldPatchItem->QTreeWidgetItem::parent())
      updatePatch(&workingInstrument, (MusECore::Patch*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
    else
      updatePatchGroup(&workingInstrument, (MusECore::PatchGroup*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
  }
  
  // We're still on the same item. No need to set oldPatchItem as in patchChanged...
  
  // If we're switching to the Controller tab, update the default patch button text in case a patch changed...
  if(QString(w->objectName()) == QString("controllerTab"))
  {
    QTreeWidgetItem* sel = viewController->currentItem();
        
    if(!sel || !sel->data(0, Qt::UserRole).value<void*>()) 
      return;
        
    MusECore::MidiController* c = (MusECore::MidiController*)sel->data(0, Qt::UserRole).value<void*>();
    MusECore::MidiController::ControllerType type = MusECore::midiControllerType(c->num());
        
    // Grab the controller number from the actual values showing
    //  and set the patch button text.
    if(type == MusECore::MidiController::Program)
      setDefaultPatchName(getDefaultPatchNumber());
  }  
}

//---------------------------------------------------------
//   patchNameReturn
//---------------------------------------------------------

void EditInstrument::patchNameReturn()
{
  QTreeWidgetItem* item = patchView->currentItem();
  
  if (item == 0)
        return;
  
  QString s = patchNameEdit->text();
  
  if(item->text(0) == s)
    return;
  
  MusECore::PatchGroupList* pg = workingInstrument.groups();
  for(MusECore::iPatchGroup g = pg->begin(); g != pg->end(); ++g) 
  {
    MusECore::PatchGroup* pgp = *g;
    // If the item has a parent, it's a patch item.
    if(item->QTreeWidgetItem::parent())
    {
      MusECore::Patch* curp = (MusECore::Patch*)item->data(0, Qt::UserRole).value<void*>();
      for(MusECore::iPatch p = pgp->patches.begin(); p != pgp->patches.end(); ++p) 
      {
        if((*p) != curp && (*p)->name == s) 
        {
          patchNameEdit->blockSignals(true);
          // Grab the last valid name from the item text, since the patch has not been updated yet.
          patchNameEdit->setText(item->text(0));
          patchNameEdit->blockSignals(false);
          
          QMessageBox::critical(this,
              tr("MusE: Bad patch name"),
              tr("Please choose a unique patch name"),
              QMessageBox::Ok,
              Qt::NoButton,
              Qt::NoButton);
              
          return;
        }
      }  
    }
    else
    // The item has no parent. It's a patch group item.
    {
      MusECore::PatchGroup* curpg = (MusECore::PatchGroup*)item->data(0, Qt::UserRole).value<void*>();
      if(pgp != curpg && pgp->name == s) 
      {
        patchNameEdit->blockSignals(true);
        // Grab the last valid name from the item text, since the patch group has not been updated yet.
        patchNameEdit->setText(item->text(0));
        patchNameEdit->blockSignals(false);
        
        QMessageBox::critical(this,
            tr("MusE: Bad patchgroup name"),
            tr("Please choose a unique patchgroup name"),
            QMessageBox::Ok,
            Qt::NoButton,
            Qt::NoButton);
            
        return;
      }
    }
  }
  
    item->setText(0, s);
    workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   patchChanged
//---------------------------------------------------------
void EditInstrument::patchChanged()
    {
      if(oldPatchItem)
      {
            if(oldPatchItem->parent())
                    updatePatch(&workingInstrument, (MusECore::Patch*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
            else
                    updatePatchGroup(&workingInstrument, (MusECore::PatchGroup*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
      }
      
      QTreeWidgetItem* sel = patchView->selectedItems().size() ? patchView->selectedItems()[0] : 0;
      oldPatchItem = sel;
      
      if(!sel || !sel->data(0, Qt::UserRole).value<void*>())
      {
        patchNameEdit->setText("");
        spinBoxHBank->setEnabled(false);
        spinBoxLBank->setEnabled(false);
        spinBoxProgram->setEnabled(false);
        checkBoxDrum->setEnabled(false);
        // REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
        //checkBoxGM->setEnabled(false);
        //checkBoxGS->setEnabled(false);
        //checkBoxXG->setEnabled(false);
        return;
      }
      
      // If the item has a parent, it's a patch item.
      if(sel->parent())
      {
        MusECore::Patch* p = (MusECore::Patch*)sel->data(0, Qt::UserRole).value<void*>();
        patchNameEdit->setText(p->name);
        spinBoxHBank->setEnabled(true);
        spinBoxLBank->setEnabled(true);
        spinBoxProgram->setEnabled(true);
        checkBoxDrum->setEnabled(true);
        // REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
        //checkBoxGM->setEnabled(true);
        //checkBoxGS->setEnabled(true);
        //checkBoxXG->setEnabled(true);
        
        int hb = ((p->hbank + 1) & 0xff);
        int lb = ((p->lbank + 1) & 0xff);
        int pr = ((p->prog + 1) & 0xff);
        spinBoxHBank->setValue(hb);
        spinBoxLBank->setValue(lb);
        spinBoxProgram->setValue(pr);
        checkBoxDrum->setChecked(p->drum);
        // REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
        //checkBoxGM->setChecked(p->typ & 1);
        //checkBoxGS->setChecked(p->typ & 2);
        //checkBoxXG->setChecked(p->typ & 4);
      }  
      else
      // The item is a patch group item.
      {
        patchNameEdit->setText( ((MusECore::PatchGroup*)sel->data(0, Qt::UserRole).value<void*>())->name );
        spinBoxHBank->setEnabled(false);
        spinBoxLBank->setEnabled(false);
        spinBoxProgram->setEnabled(false);
        checkBoxDrum->setEnabled(false);
        // REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
        //checkBoxGM->setEnabled(false);
        //checkBoxGS->setEnabled(false);
        //checkBoxXG->setEnabled(false);
      }  
    }

//---------------------------------------------------------
//   defPatchChanged
//---------------------------------------------------------

void EditInstrument::defPatchChanged(int)
{
      QTreeWidgetItem* item = viewController->currentItem();
      
      if (!item)
            return;
        
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      
      int val = getDefaultPatchNumber();
      
      c->setInitVal(val);
      
      setDefaultPatchName(val);
      
      item->setText(COL_DEF, getPatchItemText(val));
      workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   patchButtonClicked
//---------------------------------------------------------

void EditInstrument::patchButtonClicked()
{
      QMenu* patchpopup = new QMenu;
      
      MusECore::PatchGroupList* pg = workingInstrument.groups();
      
      if (pg->size() > 1) {
            for (MusECore::ciPatchGroup i = pg->begin(); i != pg->end(); ++i) {
                  MusECore::PatchGroup* pgp = *i;
                  QMenu* pm = patchpopup->addMenu(pgp->name);
                  //pm->setCheckable(false);//Qt4 doc says this is unnecessary
                  pm->setFont(MusEGlobal::config.fonts[0]);
                  const MusECore::PatchList& pl = pgp->patches;
                  for (MusECore::ciPatch ipl = pl.begin(); ipl != pl.end(); ++ipl) {
                        const MusECore::Patch* mp = *ipl;
                              int id = ((mp->hbank & 0xff) << 16)
                                         + ((mp->lbank & 0xff) << 8) + (mp->prog & 0xff);
                              QAction *ac1 = pm->addAction(mp->name);
                              ac1->setData(id);
                        }
                  }
            }
      else if (pg->size() == 1 ){
            // no groups
            const MusECore::PatchList& pl = pg->front()->patches;
            for (MusECore::ciPatch ipl = pl.begin(); ipl != pl.end(); ++ipl) {
                  const MusECore::Patch* mp = *ipl;
                        int id = ((mp->hbank & 0xff) << 16)
                                 + ((mp->lbank & 0xff) << 8) + (mp->prog & 0xff);
                        QAction *ac2 = patchpopup->addAction(mp->name);
                        ac2->setData(id);
                  }
            }

      if(patchpopup->actions().count() == 0)
      {
        delete patchpopup;
        return;
      }
      
      QAction* act = patchpopup->exec(patchButton->mapToGlobal(QPoint(10,5)));
      if(!act)
      {
        delete patchpopup;
        return;
      }
      
      int rv = act->data().toInt();
      delete patchpopup;

      if (rv != -1) 
      {
        setDefaultPatchControls(rv);
        
        QTreeWidgetItem* item = viewController->currentItem();

        if(item)
        {
          MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
          c->setInitVal(rv);
          
          item->setText(COL_DEF, getPatchItemText(rv));
        }
        workingInstrument.setDirty(true);
      }
            
}

//---------------------------------------------------------
//   addControllerToView
//---------------------------------------------------------

QTreeWidgetItem* EditInstrument::addControllerToView(MusECore::MidiController* mctrl)
{
      QString hnum;
      QString lnum;
      QString min;
      QString max;
      QString def;
      int defval = mctrl->initVal();
      int n = mctrl->num();
      int h = (n >> 8) & 0x7f;
      int l = n & 0x7f;
      if((n & 0xff) == 0xff)
        l = -1;
        
      MusECore::MidiController::ControllerType t = MusECore::midiControllerType(n);
      switch(t)
      {
          case MusECore::MidiController::Controller7:
                hnum = "---";
                if(l == -1)
                  lnum = "*";
                else  
                  lnum.setNum(l);
                min.setNum(mctrl->minVal());
                max.setNum(mctrl->maxVal());
                if(defval == MusECore::CTRL_VAL_UNKNOWN)
                  def = "---";
                else
                  def.setNum(defval);
                break;
          case MusECore::MidiController::RPN:
          case MusECore::MidiController::NRPN:
          case MusECore::MidiController::RPN14:
          case MusECore::MidiController::NRPN14:
          case MusECore::MidiController::Controller14:
                hnum.setNum(h);
                if(l == -1)
                  lnum = "*";
                else  
                  lnum.setNum(l);
                min.setNum(mctrl->minVal());
                max.setNum(mctrl->maxVal());
                if(defval == MusECore::CTRL_VAL_UNKNOWN)
                  def = "---";
                else
                  def.setNum(defval);
                break;
          case MusECore::MidiController::Pitch:
          case MusECore::MidiController::PolyAftertouch:
          case MusECore::MidiController::Aftertouch:
                hnum = "---";
                lnum = "---";
                min.setNum(mctrl->minVal());
                max.setNum(mctrl->maxVal());
                if(defval == MusECore::CTRL_VAL_UNKNOWN)
                  def = "---";
                else
                  def.setNum(defval);
                break;
          case MusECore::MidiController::Program:
                hnum = "---";
                lnum = "---";
                min = "---";
                max = "---";
                def = getPatchItemText(defval); 
                break;
                
          default:
                hnum = "---";
                lnum = "---";
                min = "---";
                max = "---";
                def = "---";
                break;
      }

      QString show_midi, show_drum;
      if(mctrl->showInTracks() & MusECore::MidiController::ShowInMidi)
        show_midi = "X";
      if(mctrl->showInTracks() & MusECore::MidiController::ShowInDrum)
        show_drum = "X";
      QTreeWidgetItem* ci =  new QTreeWidgetItem(viewController,
          QStringList() <<  mctrl->name() << int2ctrlType(t) << hnum << lnum << min << max << def << show_midi << show_drum);
      ci->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
      ci->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
      for(int i = 2; i < 9; ++i)
        ci->setTextAlignment(i, Qt::AlignRight | Qt::AlignVCenter);
      QVariant v = qVariantFromValue((void*)(mctrl));
      ci->setData(0, Qt::UserRole, v);
      
      return ci;
}
      
//---------------------------------------------------------
//   controllerChanged
//---------------------------------------------------------

void EditInstrument::controllerChanged()
      {
	QTreeWidgetItem* sel = viewController->selectedItems().size() ? viewController->selectedItems()[0] : 0;
      
	if(!sel || !sel->data(0, Qt::UserRole).value<void*>()) 
      {
        ctrlName->blockSignals(true);
        ctrlName->setText("");
        ctrlName->blockSignals(false);
        return;
      }
      
      MusECore::MidiController* c = (MusECore::MidiController*)sel->data(0, Qt::UserRole).value<void*>();
      
      ctrlName->blockSignals(true);
      ctrlName->setText(c->name());
      ctrlName->blockSignals(false);
      
      int ctrlH = (c->num() >> 8) & 0x7f;
      int ctrlL = c->num() & 0x7f;
      if(c->isPerNoteController())
        ctrlL = -1;
        
      MusECore::MidiController::ControllerType type = MusECore::midiControllerType(c->num());
      int idx = ctrlType->findData(type);
      if(idx != -1)
      {
        ctrlType->blockSignals(true);
        ctrlType->setCurrentIndex(idx);
        ctrlType->blockSignals(false);
      }

      ctrlShowInMidi->setChecked(c->showInTracks() & MusECore::MidiController::ShowInMidi);
      ctrlShowInDrum->setChecked(c->showInTracks() & MusECore::MidiController::ShowInDrum);
      
      spinBoxHCtrlNo->blockSignals(true);
      spinBoxLCtrlNo->blockSignals(true);
      spinBoxMin->blockSignals(true);
      spinBoxMax->blockSignals(true);
      spinBoxDefault->blockSignals(true);
     
      switch (type) {
            case MusECore::MidiController::Controller7:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(true);
                  spinBoxHCtrlNo->setValue(0);
                  spinBoxLCtrlNo->setValue(ctrlL);
                  spinBoxMin->setEnabled(true);
                  spinBoxMax->setEnabled(true);
                  spinBoxMin->setRange(-128, 127);
                  spinBoxMax->setRange(-128, 127);
                  spinBoxMin->setValue(c->minVal());
                  spinBoxMax->setValue(c->maxVal());
                  enableDefaultControls(true, false);
                  break;
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
                  spinBoxHCtrlNo->setEnabled(true);
                  spinBoxLCtrlNo->setEnabled(true);
                  spinBoxHCtrlNo->setValue(ctrlH);
                  spinBoxLCtrlNo->setValue(ctrlL);
                  spinBoxMin->setEnabled(true);
                  spinBoxMax->setEnabled(true);
                  spinBoxMin->setRange(-128, 127);
                  spinBoxMax->setRange(-128, 127);
                  spinBoxMin->setValue(c->minVal());
                  spinBoxMax->setValue(c->maxVal());
                  enableDefaultControls(true, false);
                  break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
                  spinBoxHCtrlNo->setEnabled(true);
                  spinBoxLCtrlNo->setEnabled(true);
                  spinBoxHCtrlNo->setValue(ctrlH);
                  spinBoxLCtrlNo->setValue(ctrlL);
                  spinBoxMin->setEnabled(true);
                  spinBoxMax->setEnabled(true);
                  spinBoxMin->setRange(-16384, 16383);
                  spinBoxMax->setRange(-16384, 16383);
                  spinBoxMin->setValue(c->minVal());
                  spinBoxMax->setValue(c->maxVal());
                  enableDefaultControls(true, false);
                  break;
            case MusECore::MidiController::Pitch:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  spinBoxHCtrlNo->setValue(0);
                  spinBoxLCtrlNo->setValue(0);
                  spinBoxMin->setEnabled(true);
                  spinBoxMax->setEnabled(true);
                  spinBoxMin->setRange(-8192, 8191);
                  spinBoxMax->setRange(-8192, 8191);
                  spinBoxMin->setValue(c->minVal());
                  spinBoxMax->setValue(c->maxVal());
                  enableDefaultControls(true, false);
                  break;
            case MusECore::MidiController::PolyAftertouch:
            case MusECore::MidiController::Aftertouch:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  spinBoxHCtrlNo->setValue(0);
                  spinBoxLCtrlNo->setValue(0);
                  spinBoxMin->setEnabled(true);
                  spinBoxMax->setEnabled(true);
                  spinBoxMin->setRange(0, 127);
                  spinBoxMax->setRange(0, 127);
                  spinBoxMin->setValue(c->minVal());
                  spinBoxMax->setValue(c->maxVal());
                  enableDefaultControls(true, false);
                  break;
            case MusECore::MidiController::Program:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  spinBoxHCtrlNo->setValue(0);
                  spinBoxLCtrlNo->setValue(0);
                  spinBoxMin->setEnabled(false);
                  spinBoxMax->setEnabled(false);
                  spinBoxMin->setRange(0, 0);
                  spinBoxMax->setRange(0, 0);
                  spinBoxMin->setValue(0);
                  spinBoxMax->setValue(0);
                  enableDefaultControls(false, true);
                  break;
            default:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  spinBoxMin->setEnabled(false);
                  spinBoxMax->setEnabled(false);
                  enableDefaultControls(false, false);
                  break;
            }      
            
      if(type == MusECore::MidiController::Program)
      {
        spinBoxDefault->setRange(0, 0);
        spinBoxDefault->setValue(0);
        setDefaultPatchControls(c->initVal());
      }
      else
      {
        spinBoxDefault->setRange(c->minVal() - 1, c->maxVal());
        if(c->initVal() == MusECore::CTRL_VAL_UNKNOWN)
          spinBoxDefault->setValue(spinBoxDefault->minimum());
        else  
          spinBoxDefault->setValue(c->initVal());
      }
      
      spinBoxHCtrlNo->blockSignals(false);
      spinBoxLCtrlNo->blockSignals(false);
      spinBoxMin->blockSignals(false);
      spinBoxMax->blockSignals(false);
      spinBoxDefault->blockSignals(false);

      ctrlValidLabel->setPixmap(*greendotIcon);
      enableNonCtrlControls(true);
      }

//---------------------------------------------------------
//   ctrlNameReturn
//---------------------------------------------------------

void EditInstrument::ctrlNameReturn()
{
      QTreeWidgetItem* item = viewController->currentItem();

      if (item == 0)
            return;
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      
      QString cName = ctrlName->text();
      
      MusECore::MidiControllerList* cl = workingInstrument.controller();
      for(MusECore::ciMidiController ic = cl->begin(); ic != cl->end(); ++ic) 
      {
        MusECore::MidiController* mc = ic->second;
        if(mc != c && mc->name() == cName) 
        {
          ctrlName->blockSignals(true);
          ctrlName->setText(c->name());
          ctrlName->blockSignals(false);
          
          QMessageBox::critical(this,
              tr("MusE: Bad controller name"),
              tr("Please choose a unique controller name"),
              QMessageBox::Ok,
              Qt::NoButton,
              Qt::NoButton);
              
          return;
        }
      }
      
      if(c->name() == cName)
        return;

      c->setName(ctrlName->text());
      item->setText(COL_CNAME, ctrlName->text());
      workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   ctrlTypeChanged
//---------------------------------------------------------

void EditInstrument::ctrlTypeChanged(int idx)
      {
      QTreeWidgetItem* item = viewController->currentItem();
      
      if (item == 0)
            return;
      
      MusECore::MidiController::ControllerType t = (MusECore::MidiController::ControllerType)ctrlType->itemData(idx).toInt();
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      int hnum = 0, lnum = 0;
      
      switch (t) {
            case MusECore::MidiController::Controller7:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(true);
                  lnum = spinBoxLCtrlNo->value();
                  break;
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
                  spinBoxHCtrlNo->setEnabled(true);
                  spinBoxLCtrlNo->setEnabled(true);
                  hnum = spinBoxHCtrlNo->value();
                  lnum = spinBoxLCtrlNo->value();
                  break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
                  spinBoxHCtrlNo->setEnabled(true);
                  spinBoxLCtrlNo->setEnabled(true);
                  hnum = spinBoxHCtrlNo->value();
                  lnum = spinBoxLCtrlNo->value();
                  break;
            case MusECore::MidiController::Pitch:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  break;
            case MusECore::MidiController::PolyAftertouch:
            case MusECore::MidiController::Aftertouch:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  break;
            case MusECore::MidiController::Program:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  break;
            // Shouldn't happen...
            default:
                  spinBoxHCtrlNo->setEnabled(false);
                  spinBoxLCtrlNo->setEnabled(false);
                  spinBoxMin->setEnabled(false);
                  spinBoxMax->setEnabled(false);
                  enableDefaultControls(false, false);
                  spinBoxMin->blockSignals(false);
                  spinBoxMax->blockSignals(false);
                  return;
                  break;
            }      
            
      int new_num = MusECore::MidiController::genNum(t, hnum, lnum);
      MusECore::MidiControllerList* cl = workingInstrument.controller();
      // Check if either a per-note controller, or else a regular controller already exists.
      if(!cl->ctrlAvailable(new_num, c))
      {
        ctrlValidLabel->setPixmap(*reddotIcon);
        enableNonCtrlControls(false);
        return;
      }
      
      ctrlValidLabel->setPixmap(*greendotIcon);
      
      if(t == MusECore::midiControllerType(c->num()))
      {  
         enableNonCtrlControls(true);
         return;
      }
      
      cl->erase(c->num());
      c->setNum(new_num);
      cl->add(c);
      
      enableNonCtrlControls(true);
      
      item->setText(COL_TYPE, ctrlType->currentText());

      spinBoxMin->blockSignals(true);
      spinBoxMax->blockSignals(true);
      spinBoxDefault->blockSignals(true);

      switch (t) {
            case MusECore::MidiController::Controller7:
                  spinBoxMin->setRange(-128, 127);
                  spinBoxMax->setRange(-128, 127);
                  spinBoxMin->setValue(0);
                  spinBoxMax->setValue(127);
                  spinBoxDefault->setRange(spinBoxMin->value() - 1, spinBoxMax->value());
                  spinBoxDefault->setValue(spinBoxDefault->minimum());
                  if(lnum == -1)
                    item->setText(COL_LNUM, QString("*"));
                  else
                    item->setText(COL_LNUM, QString().setNum(lnum));
                  item->setText(COL_HNUM, QString("---"));
                  item->setText(COL_MIN, QString().setNum(spinBoxMin->value()));
                  item->setText(COL_MAX, QString().setNum(spinBoxMax->value()));
                  item->setText(COL_DEF, QString("---"));
                  break;
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
                  spinBoxMin->setRange(-128, 127);
                  spinBoxMax->setRange(-128, 127);
                  spinBoxMin->setValue(0);
                  spinBoxMax->setValue(127);
                  spinBoxDefault->setRange(spinBoxMin->value() - 1, spinBoxMax->value());
                  spinBoxDefault->setValue(spinBoxDefault->minimum());
                  if(lnum == -1)
                    item->setText(COL_LNUM, QString("*"));
                  else
                    item->setText(COL_LNUM, QString().setNum(lnum));
                  item->setText(COL_HNUM, QString().setNum(hnum));
                  item->setText(COL_MIN, QString().setNum(spinBoxMin->value()));
                  item->setText(COL_MAX, QString().setNum(spinBoxMax->value()));
                  item->setText(COL_DEF, QString("---"));
                  break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
                  spinBoxMin->setRange(-16384, 16383);
                  spinBoxMax->setRange(-16384, 16383);
                  spinBoxMin->setValue(0);
                  spinBoxMax->setValue(16383);
                  spinBoxDefault->setRange(spinBoxMin->value() - 1, spinBoxMax->value());
                  spinBoxDefault->setValue(spinBoxDefault->minimum());
                  if(lnum == -1)
                    item->setText(COL_LNUM, QString("*"));
                  else
                    item->setText(COL_LNUM, QString().setNum(lnum));
                  item->setText(COL_HNUM, QString().setNum(hnum));
                  item->setText(COL_MIN, QString().setNum(spinBoxMin->value()));
                  item->setText(COL_MAX, QString().setNum(spinBoxMax->value()));
                  item->setText(COL_DEF, QString("---"));
                  break;
            case MusECore::MidiController::Pitch:
                  spinBoxMin->setRange(-8192, 8191);
                  spinBoxMax->setRange(-8192, 8191);
                  spinBoxMin->setValue(-8192);
                  spinBoxMax->setValue(8191);
                  spinBoxDefault->setRange(spinBoxMin->value() - 1, spinBoxMax->value());
                  spinBoxDefault->setValue(spinBoxDefault->minimum());
                  item->setText(COL_LNUM, QString("---"));
                  item->setText(COL_HNUM, QString("---"));
                  item->setText(COL_MIN, QString().setNum(spinBoxMin->value()));
                  item->setText(COL_MAX, QString().setNum(spinBoxMax->value()));
                  item->setText(COL_DEF, QString("---"));
                  break;
            case MusECore::MidiController::PolyAftertouch:
            case MusECore::MidiController::Aftertouch:
                  spinBoxMin->setRange(0, 127);
                  spinBoxMax->setRange(0, 127);
                  spinBoxMin->setValue(0);
                  spinBoxMax->setValue(127);
                  spinBoxDefault->setRange(spinBoxMin->value() - 1, spinBoxMax->value());
                  spinBoxDefault->setValue(spinBoxDefault->minimum());
                  item->setText(COL_LNUM, QString("---"));
                  item->setText(COL_HNUM, QString("---"));
                  item->setText(COL_MIN, QString().setNum(spinBoxMin->value()));
                  item->setText(COL_MAX, QString().setNum(spinBoxMax->value()));
                  item->setText(COL_DEF, QString("---"));
                  break;
            case MusECore::MidiController::Program:
                  spinBoxMin->setRange(0, 0);
                  spinBoxMax->setRange(0, 0);
                  spinBoxMin->setValue(0);
                  spinBoxMax->setValue(0);
                  spinBoxDefault->setRange(0, 0);
                  spinBoxDefault->setValue(0);
                  item->setText(COL_LNUM, QString("---"));
                  item->setText(COL_HNUM, QString("---"));
                  item->setText(COL_MIN, QString("---"));
                  item->setText(COL_MAX, QString("---"));
                  item->setText(COL_DEF, QString("---"));
                  break;
            // Shouldn't happen...
            default:
                  return;
                  break;
            }

      spinBoxMin->blockSignals(false);
      spinBoxMax->blockSignals(false);
      spinBoxDefault->blockSignals(false);


      setDefaultPatchControls(0xffffff);
      if(t == MusECore::MidiController::Program)
      {
        c->setMinVal(0);
        c->setMaxVal(0xffffff);
        c->setInitVal(0xffffff);
      }
      else
      {
        c->setMinVal(spinBoxMin->value());
        c->setMaxVal(spinBoxMax->value());
        if(spinBoxDefault->value() == spinBoxDefault->minimum())
          c->setInitVal(MusECore::CTRL_VAL_UNKNOWN);
        else  
          c->setInitVal(spinBoxDefault->value());
      }  
      
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   ctrlShowInMidiChanged
//---------------------------------------------------------

void EditInstrument::ctrlShowInMidiChanged(int state)
      {
      QTreeWidgetItem* item = viewController->currentItem();
      if (item == 0)
            return;
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      int show = c->showInTracks();
      if((show & MusECore::MidiController::ShowInMidi) == (state == Qt::Checked))
        return;
      if(state == Qt::Checked)
      {
        c->setShowInTracks(show | MusECore::MidiController::ShowInMidi);
        item->setText(COL_SHOW_MIDI, "X");
      }
      else
      {
        c->setShowInTracks(show & ~MusECore::MidiController::ShowInMidi);
        item->setText(COL_SHOW_MIDI, "");
      }
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   ctrlShowInMidiChanged
//---------------------------------------------------------

void EditInstrument::ctrlShowInDrumChanged(int state)
      {
      QTreeWidgetItem* item = viewController->currentItem();
      if (item == 0)
            return;
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      int show = c->showInTracks();
      if((show & MusECore::MidiController::ShowInDrum) == (state == Qt::Checked))
        return;
      if(state == Qt::Checked)
      {
        c->setShowInTracks(show | MusECore::MidiController::ShowInDrum);
        item->setText(COL_SHOW_DRUM, "X");
      }
      else
      {
        c->setShowInTracks(show & ~MusECore::MidiController::ShowInDrum);
        item->setText(COL_SHOW_DRUM, "");
      }
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   ctrlNumChanged
//---------------------------------------------------------

void EditInstrument::ctrlNumChanged()
      {
      QTreeWidgetItem* item = viewController->currentItem();
      if (item == 0 || ctrlType->currentIndex() == -1)
            return;
      MusECore::MidiController::ControllerType t = (MusECore::MidiController::ControllerType)ctrlType->itemData(ctrlType->currentIndex()).toInt(); 
      int hnum = 0, lnum = 0;
      switch (t) {
            case MusECore::MidiController::Controller7:
                  lnum = spinBoxLCtrlNo->value();
                  break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
                  hnum = spinBoxHCtrlNo->value();
                  lnum = spinBoxLCtrlNo->value();
                  break;
            // Should not happen...
            case MusECore::MidiController::Pitch:
            case MusECore::MidiController::PolyAftertouch:
            case MusECore::MidiController::Aftertouch:
            case MusECore::MidiController::Program:
            case MusECore::MidiController::Velo:
                  return;
            default:
                  printf("EditInstrument::ctrlNumChanged Error: Unknown control type\n");
                  return;
                  break;
            }
            
      int new_num = MusECore::MidiController::genNum(t, hnum, lnum);
      if(new_num == -1)
      {
        printf("EditInstrument::ctrlNumChanged Error: genNum returned -1\n");
        return;
      }
      
      MusECore::MidiControllerList* cl = workingInstrument.controller();
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();

      // Check if either a per-note controller, or else a regular controller already exists.
      if(!cl->ctrlAvailable(new_num, c))
      {
        ctrlValidLabel->setPixmap(*reddotIcon);
        enableNonCtrlControls(false);
        return;
      }
      ctrlValidLabel->setPixmap(*greendotIcon);
      enableNonCtrlControls(true);
      if(cl->erase(c->num()) == 0)
        printf("EditInstrument::ctrlNumChanged Warning: Erase failed! Proceeding anyway.\n");
      c->setNum(new_num);
      cl->add(c);
      QString s;
      if(c->isPerNoteController())
        item->setText(COL_LNUM, QString("*"));
      else {
        s.setNum(lnum);
        item->setText(COL_LNUM, s);
        }
      switch (t) {
            case MusECore::MidiController::Controller7:
                  item->setText(COL_HNUM, "---");
                  break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
                  s.setNum(hnum);
                  item->setText(COL_HNUM, s);
                  break;
            default:
                  return;
            }
      item->setText(COL_TYPE, ctrlType->currentText());
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   ctrlMinChanged
//---------------------------------------------------------

void EditInstrument::ctrlMinChanged(int val)
{
      QTreeWidgetItem* item = viewController->currentItem();
      
      if (item == 0)
            return;
        
      QString s;
      s.setNum(val);
      item->setText(COL_MIN, s);
      
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      c->setMinVal(val);
      
      int rng = 0;
      switch(MusECore::midiControllerType(c->num()))
      {
            case MusECore::MidiController::Controller7:
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
            case MusECore::MidiController::PolyAftertouch:
            case MusECore::MidiController::Aftertouch:
                  rng = 127;
                  break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
            case MusECore::MidiController::Pitch:
                  rng = 16383;
                  break;
            default: 
                  break;      
      }
      
      int mx = c->maxVal();
      
      if(val > mx)
      {
        c->setMaxVal(val);
        spinBoxMax->blockSignals(true);
        spinBoxMax->setValue(val);
        spinBoxMax->blockSignals(false);
        item->setText(COL_MAX, s);
      }  
      else
      if(mx - val > rng)
      {
        mx = val + rng;
        c->setMaxVal(mx);
        spinBoxMax->blockSignals(true);
        spinBoxMax->setValue(mx);
        spinBoxMax->blockSignals(false);
        item->setText(COL_MAX, QString().setNum(mx));
      }  
      
      spinBoxDefault->blockSignals(true);
      
      spinBoxDefault->setRange(spinBoxMin->value() - 1, spinBoxMax->value());
      
      int inval = c->initVal();
      if(inval == MusECore::CTRL_VAL_UNKNOWN)
        spinBoxDefault->setValue(spinBoxDefault->minimum());
      else  
      {
        if(inval < c->minVal())
        {
          c->setInitVal(c->minVal());
          spinBoxDefault->setValue(c->minVal());
        }
        else  
        if(inval > c->maxVal())
        {  
          c->setInitVal(c->maxVal());
          spinBoxDefault->setValue(c->maxVal());
        }  
      } 
      
      spinBoxDefault->blockSignals(false);
      
      workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   ctrlMaxChanged
//---------------------------------------------------------

void EditInstrument::ctrlMaxChanged(int val)
{
      QTreeWidgetItem* item = viewController->currentItem();
      
      if (item == 0)
            return;
        
      QString s;
      s.setNum(val);
      item->setText(COL_MAX, s);
      
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      c->setMaxVal(val);
      
      int rng = 0;
      switch(MusECore::midiControllerType(c->num()))
      {
            case MusECore::MidiController::Controller7:
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
            case MusECore::MidiController::PolyAftertouch:
            case MusECore::MidiController::Aftertouch:
                  rng = 127;
                  break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
            case MusECore::MidiController::Pitch:
                  rng = 16383;
                  break;
            default: 
                  break;      
      }
      
      int mn = c->minVal();
      
      if(val < mn)
      {
        c->setMinVal(val);
        spinBoxMin->blockSignals(true);
        spinBoxMin->setValue(val);
        spinBoxMin->blockSignals(false);
        item->setText(COL_MIN, s);
      }  
      else
      if(val - mn > rng)
      {
        mn = val - rng;
        c->setMinVal(mn);
        spinBoxMin->blockSignals(true);
        spinBoxMin->setValue(mn);
        spinBoxMin->blockSignals(false);
        item->setText(COL_MIN, QString().setNum(mn));
      }  
      
      spinBoxDefault->blockSignals(true);
      
      spinBoxDefault->setRange(spinBoxMin->value() - 1, spinBoxMax->value());
      
      int inval = c->initVal();
      if(inval == MusECore::CTRL_VAL_UNKNOWN)
        spinBoxDefault->setValue(spinBoxDefault->minimum());
      else  
      {
        if(inval < c->minVal())
        {
          c->setInitVal(c->minVal());
          spinBoxDefault->setValue(c->minVal());
        }
        else  
        if(inval > c->maxVal())
        {  
          c->setInitVal(c->maxVal());
          spinBoxDefault->setValue(c->maxVal());
        }  
      } 
      
      spinBoxDefault->blockSignals(false);
      
      workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   ctrlDefaultChanged
//---------------------------------------------------------

void EditInstrument::ctrlDefaultChanged(int val)
{
      QTreeWidgetItem* item = viewController->currentItem();

      if (item == 0)
            return;
        
      MusECore::MidiController* c = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      
      if(val == c->minVal() - 1)
      {
        c->setInitVal(MusECore::CTRL_VAL_UNKNOWN);
        item->setText(COL_DEF, QString("---"));
      }  
      else
      {
        c->setInitVal(val);
        item->setText(COL_DEF, QString().setNum(val));
      }
      workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   ctrlNullParamHChanged
//---------------------------------------------------------

void EditInstrument::ctrlNullParamHChanged(int nvh)
{
  int nvl = nullParamSpinBoxL->value();
  if(nvh == -1)
  {
    nullParamSpinBoxL->blockSignals(true);
    nullParamSpinBoxL->setValue(-1);
    nullParamSpinBoxL->blockSignals(false);
    nvl = -1;
  }
  else
  {
    if(nvl == -1)
    {
      nullParamSpinBoxL->blockSignals(true);
      nullParamSpinBoxL->setValue(0);
      nullParamSpinBoxL->blockSignals(false);
      nvl = 0;
    }
  }
  if(nvh == -1 && nvl == -1)
    workingInstrument.setNullSendValue(-1);
  else  
    workingInstrument.setNullSendValue((nvh << 8) | nvl);
  workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   ctrlNullParamLChanged
//---------------------------------------------------------

void EditInstrument::ctrlNullParamLChanged(int nvl)
{
  int nvh = nullParamSpinBoxH->value();
  if(nvl == -1)
  {
    nullParamSpinBoxH->blockSignals(true);
    nullParamSpinBoxH->setValue(-1);
    nullParamSpinBoxH->blockSignals(false);
    nvh = -1;
  }
  else
  {
    if(nvh == -1)
    {
      nullParamSpinBoxH->blockSignals(true);
      nullParamSpinBoxH->setValue(0);
      nullParamSpinBoxH->blockSignals(false);
      nvh = 0;
    }
  }
  if(nvh == -1 && nvl == -1)
    workingInstrument.setNullSendValue(-1);
  else  
    workingInstrument.setNullSendValue((nvh << 8) | nvl);
  workingInstrument.setDirty(true);
}

//---------------------------------------------------------
//   updateSysex
//---------------------------------------------------------

void EditInstrument::updateSysex(MusECore::MidiInstrument* instrument, MusECore::SysEx* so)
      {
      if (sysexName->text() != so->name) {
            so->name = sysexName->text();
            instrument->setDirty(true);
            }
      if (sysexComment->toPlainText() != so->comment) {
            so->comment = sysexComment->toPlainText();
            instrument->setDirty(true);
            }
      unsigned char* data;
      int len = MusECore::string2sysex(sysexData->toPlainText(), &data);
      if (len == -1) // Conversion unsuccessful?  
      {
        QMessageBox::information(0,
                                  QString("MusE"),
                                  QWidget::tr("Cannot convert sysex string"));
        return;
      }
      if(so->dataLen != len || memcmp(data, so->data, len) != 0) {
            if(so->dataLen != 0 && so->data)
              delete[] so->data;
            so->data = data;
            so->dataLen = len;
            instrument->setDirty(true);
            }
      }

//---------------------------------------------------------
//   sysexChanged
//---------------------------------------------------------

void EditInstrument::sysexChanged(QListWidgetItem* sel, QListWidgetItem* old)
      {
      if (old) {
            MusECore::SysEx* so = (MusECore::SysEx*)old->data(Qt::UserRole).value<void*>();
            updateSysex(&workingInstrument, so);
            }
      if (sel == 0) {
            sysexName->setText("");
            sysexComment->setText("");
            sysexData->setText("");
            sysexName->setEnabled(false);
            sysexComment->setEnabled(false);
            sysexData->setEnabled(false);
            return;
            }
      sysexName->setEnabled(true);
      sysexComment->setEnabled(true);
      sysexData->setEnabled(true);

      MusECore::SysEx* sx = (MusECore::SysEx*)sel->data(Qt::UserRole).value<void*>();
      sysexName->setText(sx->name);
      sysexComment->setText(sx->comment);
      sysexData->setText(MusECore::sysex2string(sx->dataLen, sx->data));
      }

//---------------------------------------------------------
//   deleteSysexClicked
//---------------------------------------------------------

void EditInstrument::deleteSysexClicked()
      {
      QListWidgetItem* item2 = sysexList->currentItem();
      if(item2 == 0)
        return;
      MusECore::SysEx* sysex  = (MusECore::SysEx*)item2->data(Qt::UserRole).value<void*>();
      workingInstrument.removeSysex(sysex);
      delete item2;
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   newSysexClicked
//---------------------------------------------------------

void EditInstrument::newSysexClicked()
      {
      QString sysexName;
      for (int i = 1;; ++i) {
            sysexName = QString("Sysex-%1").arg(i);
      
            bool found = false;
            foreach(const MusECore::SysEx* s, workingInstrument.sysex()) {
                  if (s->name == sysexName) {
                        found = true;
                        break;
                        }
                  }
            if (!found)
                  break;
            }
      MusECore::SysEx* nsysex = new MusECore::SysEx;
      nsysex->name = sysexName;
      workingInstrument.addSysex(nsysex);

      QListWidgetItem* item = new QListWidgetItem(sysexName);
      QVariant v = QVariant::fromValue((void*)nsysex);
      item->setData(Qt::UserRole, v);
      sysexList->addItem(item);
      sysexList->setCurrentItem(item);
      workingInstrument.setDirty(true);

      }

//---------------------------------------------------------
//   deletePatchClicked
//---------------------------------------------------------

void EditInstrument::deletePatchClicked()
      {
      QTreeWidgetItem* pi = patchView->currentItem();

      if (pi == 0)
            return;
      
      // If the item has a parent item, it's a patch item...
      if(pi->parent())
      {
        MusECore::PatchGroup* group = (MusECore::PatchGroup*)(pi->parent())->data(0, Qt::UserRole).value<void*>();
        
        // If there is an allocated patch in the data, delete it.
        MusECore::Patch* patch = (MusECore::Patch*)pi->data(0, Qt::UserRole).value<void*>();
        if(patch)
        {
          if(group)
            group->patches.remove(patch);

          delete patch;
        }  
      }
      else
      // The item has no parent item, it's a patch group item...
      {
        // Is there an allocated patch group in the data?
        MusECore::PatchGroup* group = (MusECore::PatchGroup*)pi->data(0, Qt::UserRole).value<void*>();
        if(group)
        {
          
          MusECore::PatchGroupList* pg = workingInstrument.groups();
          for(MusECore::iPatchGroup ipg = pg->begin(); ipg != pg->end(); ++ipg)
          {
            
            if(*ipg == group)
            {
              pg->erase(ipg);
              break;
            }  
          }
          
              const MusECore::PatchList& pl = group->patches;
              for(MusECore::ciPatch ip = pl.begin(); ip != pl.end(); ++ip)
              {
                // Delete the patch.
                if(*ip)
                  delete *ip;  
              }
              
          // Now delete the group.
          delete group;
          
        }  
      }
      
      // Now delete the patch or group item (and any child patch items) from the list view tree.
      // !!! This will trigger a patchChanged call. 
      patchView->blockSignals(true);
      delete pi;
      if(patchView->currentItem())
        patchView->currentItem()->setSelected(true);
      patchView->blockSignals(false);
      
      oldPatchItem = 0;
      patchChanged();
      
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   newPatchClicked
//---------------------------------------------------------

void EditInstrument::newPatchClicked()
      {
      if(oldPatchItem)
      {
        if(oldPatchItem->parent())
          updatePatch(&workingInstrument, (MusECore::Patch*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
        else  
          updatePatchGroup(&workingInstrument, (MusECore::PatchGroup*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
      }  

      MusECore::PatchGroupList* pg = workingInstrument.groups();
      QString patchName;
      for (int i = 1;; ++i) {
            patchName = QString("Patch-%1").arg(i);
            bool found = false;

            for (MusECore::iPatchGroup g = pg->begin(); g != pg->end(); ++g) {
                  MusECore::PatchGroup* pgp = *g;
                  for (MusECore::iPatch p = pgp->patches.begin(); p != pgp->patches.end(); ++p) {
                        if ((*p)->name == patchName) {
                              found = true;
                              break;
                              }
                        }
                  if (found)
                        break;
                  }
            if (!found)
                  break;
            }

      // search current patch group
      QTreeWidgetItem* pi = patchView->currentItem();

      if (pi == 0)
            return;
      
      MusECore::Patch* selpatch = 0;
      
      // If there is a parent item then pi is a patch item, and there must be a parent patch group item.
      if(pi->parent())
      {
        // Remember the current selected patch.
        selpatch = (MusECore::Patch*)pi->data(0, Qt::UserRole).value<void*>();
        
        pi = pi->parent();
      }
      
      MusECore::PatchGroup* group = (MusECore::PatchGroup*)pi->data(0, Qt::UserRole).value<void*>();
      if(!group)
        return;
        
      
      // Create a new Patch, then store its pointer in a new patch item, 
      //  to be added later to the patch group only upon save...
      //Patch patch;
      //patch.name = patchName;
      MusECore::Patch* patch = new MusECore::Patch;
      int hb  = -1;
      int lb  = -1;
      int prg = 0;
      patch->hbank = hb;
      patch->lbank = lb;
      patch->prog = prg;
      //patch->typ = -1;                     
      patch->drum = false;
      
      if(selpatch)
      {
        hb  = selpatch->hbank;
        lb  = selpatch->lbank;
        prg = selpatch->prog;
        //patch->typ = selpatch->typ;                     
        patch->drum = selpatch->drum;                     
      }
      
      bool found = false;
      
      // The 129 is to accommodate -1 values. Yes, it may cause one extra redundant loop but hey, 
      //  if it hasn't found an available patch number by then, another loop won't matter.
      for(int k = 0; k < 129; ++k)
      {
        for(int j = 0; j < 129; ++j)
        {
          for(int i = 0; i < 128; ++i) 
          {
            found = false;

            for(MusECore::iPatchGroup g = pg->begin(); g != pg->end(); ++g) 
            {
              MusECore::PatchGroup* pgp = *g;
              for(MusECore::iPatch ip = pgp->patches.begin(); ip != pgp->patches.end(); ++ip) 
              {
                MusECore::Patch* p = *ip;
                if((p->prog  == ((prg + i) & 0x7f)) && 
                   ((p->lbank == -1 && lb == -1) || (p->lbank == ((lb + j) & 0x7f))) && 
                   ((p->hbank == -1 && hb == -1) || (p->hbank == ((hb + k) & 0x7f)))) 
                {
                  found = true;
                  break;
                }
              }
              if(found)
                break;
            }
            
            if(!found)
            {
              patch->prog  = (prg + i) & 0x7f;
              if(lb == -1)
                patch->lbank = -1;
              else  
                patch->lbank = (lb + j) & 0x7f;
                
              if(hb == -1)
                patch->hbank = -1;
              else    
                patch->hbank = (hb + k) & 0x7f;
                
              break;
            } 
              
          }
          if(!found)
            break;
        }  
        if(!found)
          break;
      }  
      
      patch->name = patchName;

      group->patches.push_back(patch);

      QTreeWidgetItem* sitem = new QTreeWidgetItem(pi);
      sitem->setText(0, patchName);
      
      patchNameEdit->setText(patchName);
      
      QVariant v = qVariantFromValue((void*)(patch));
      sitem->setData(0, Qt::UserRole, v);
      
      // May cause patchChanged call.
      patchView->blockSignals(true);
      sitem->setSelected(true);
      patchView->scrollToItem((QTreeWidgetItem*)sitem, QAbstractItemView::EnsureVisible);
      patchView->blockSignals(false);
      
      spinBoxHBank->setEnabled(true);
      spinBoxLBank->setEnabled(true);
      spinBoxProgram->setEnabled(true);
      checkBoxDrum->setEnabled(true);
      // REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
      //checkBoxGM->setEnabled(true);
      //checkBoxGS->setEnabled(true);
      //checkBoxXG->setEnabled(true);
      
      oldPatchItem = 0;
      patchChanged();
      
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   newGroupClicked
//---------------------------------------------------------

void EditInstrument::newGroupClicked()
      {
      if(oldPatchItem)
      {
        if(oldPatchItem->parent())
          updatePatch(&workingInstrument, (MusECore::Patch*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
        else  
          updatePatchGroup(&workingInstrument, (MusECore::PatchGroup*)oldPatchItem->data(0, Qt::UserRole).value<void*>());
      }  
      
      MusECore::PatchGroupList* pg = workingInstrument.groups();
      QString groupName;
      for (int i = 1;; ++i) {
            groupName = QString("Group-%1").arg(i);
            bool found = false;

            for (MusECore::ciPatchGroup g = pg->begin(); g != pg->end(); ++g) {
                  if ((*g)->name == groupName) {
                        found = true;
                        break;
                        }
                  }
            if (!found)
                  break;
            }

      // Create a new PatchGroup, then store its pointer in a new patch group item, 
      //  to be added later to the instrument only upon save...
      MusECore::PatchGroup* group = new MusECore::PatchGroup;
      group->name = groupName;
      
      pg->push_back(group);
      
      QTreeWidgetItem* sitem = new QTreeWidgetItem(patchView);
      sitem->setText(0, groupName);
      
      patchNameEdit->setText(groupName);
      
      // Set the list view item's data. 
      QVariant v = qVariantFromValue((void*)(group));
      sitem->setData(0, Qt::UserRole, v);
      //sitem->setAuxData((void*)pgp);
      
      // May cause patchChanged call.
      patchView->blockSignals(true);
      sitem->setSelected(true);
      patchView->blockSignals(false);
      
      oldPatchItem = sitem;
      
      spinBoxHBank->setEnabled(false);
      spinBoxLBank->setEnabled(false);
      spinBoxProgram->setEnabled(false);
      checkBoxDrum->setEnabled(false);
      // REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
      //checkBoxGM->setEnabled(false);
      //checkBoxGS->setEnabled(false);
      //checkBoxXG->setEnabled(false);
      
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   deleteControllerClicked
//---------------------------------------------------------

void EditInstrument::deleteControllerClicked()
      {
      QTreeWidgetItem* item = viewController->currentItem();
      
      if(!item)
        return;
      
      MusECore::MidiController* ctrl = (MusECore::MidiController*)item->data(0, Qt::UserRole).value<void*>();
      if(!ctrl)
        return;
        
      workingInstrument.controller()->erase(ctrl->num());   
      // Now delete the controller.
      delete ctrl;
      
      // Now remove the controller item from the list.
      // This may cause a controllerChanged call.
      viewController->blockSignals(true);
      delete item;
      if(viewController->currentItem())
        viewController->currentItem()->setSelected(true);
      viewController->blockSignals(false);
      
      controllerChanged();
      
      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   newControllerClicked
//---------------------------------------------------------

void EditInstrument::newControllerClicked()
      {      
      QString cName;
      MusECore::MidiControllerList* cl = workingInstrument.controller();
      for (int i = 1;; ++i) {
            cName = QString("Controller-%1").arg(i);
            bool found = false;
            for (MusECore::iMidiController ic = cl->begin(); ic != cl->end(); ++ic) {
                  MusECore::MidiController* c = ic->second;
                  if (c->name() == cName) {
                        found = true;
                        break;
                        }
                  }
            if (!found)
              break;
          }

      MusECore::MidiController* ctrl = new MusECore::MidiController();
      ctrl->setNum(MusECore::CTRL_MODULATION);
      ctrl->setMinVal(0);
      ctrl->setMaxVal(127);
      ctrl->setInitVal(MusECore::CTRL_VAL_UNKNOWN);

      QTreeWidgetItem* ci = viewController->currentItem();

      int l = 0;
      int h = 0;
      int hmax = 0x100;
      // To allow for quick multiple successive controller creation.
      // If there's a current controller item selected, copy initial values from it.
      if(ci)
      {
        MusECore::MidiController* selctl = (MusECore::MidiController*)ci->data(0, Qt::UserRole).value<void*>();
        // Ignore internal controllers and wild cards.
        if(((selctl->num() & 0xff0000) != MusECore::CTRL_INTERNAL_OFFSET) && !selctl->isPerNoteController())
        {
          switch(MusECore::midiControllerType(selctl->num()))
          {
            case MusECore::MidiController::Controller7:
              // Auto increment controller number.
              l = selctl->num() & 0x7f;
              *ctrl = *selctl; // Assign.
              break;
            case MusECore::MidiController::Controller14:
            case MusECore::MidiController::RPN:
            case MusECore::MidiController::NRPN:
            case MusECore::MidiController::RPN14:
            case MusECore::MidiController::NRPN14:
              // Auto increment controller number.
              l = selctl->num() & 0x7f;
              h = selctl->num() & 0xffffff00;
              *ctrl = *selctl; // Assign.
              break;
            // Don't duplicate these types.
            case MusECore::MidiController::Pitch:
            case MusECore::MidiController::Program:
            case MusECore::MidiController::PolyAftertouch:
            case MusECore::MidiController::Aftertouch:
            case MusECore::MidiController::Velo:
              break;
            default:
              printf("error: newControllerClicked: Unknown control type!\n");
              delete ctrl;
              return;
          }
        }
      }

      bool found = false;
      for(int k = (h & 0xffff0000); k < MusECore::CTRL_NONE_OFFSET; k += 0x10000)
      {
        // Don't copy internal controllers.
        if(k == MusECore::CTRL_INTERNAL_OFFSET)  
        {
          found = true;
          continue;
        }
        if(k == 0)
          // We're currently within the Controller7 group, limit the hi-number loop to one go (j = 0).
          hmax = 0x100;
        else
          // All other relevant controllers use hi and lo numbers.
          hmax = 0x10000;
        for(int j = 0; j < hmax; j += 0x100)
        {
          for(int i = 0; i < 128; ++i)
          {
            int num = ((i + l) & 0x7f) | ((h + j) & 0x7f00) | k;
            found = false;
            // First check if there's already a per-note controller for this control number.
            if(cl->find(num | 0xff) != cl->end())
            {
              found = true;
              break;        // Next outer loop (hi-number) iteration...
            }
            // Now check if the actual control number is NOT already taken.
            if(cl->find(num) == cl->end())
            {
              ctrl->setNum(num);
              break;
            }
            // Actual number was also taken. Keep iterating lo-number...
            found = true;
          }
          if(!found)
            break;
        }
        if(!found)
          break;
      }

      if(found) 
      {
        QMessageBox::critical(this, tr("New controller: Error"), tr("Error! All control numbers are taken up!\nClean up the instrument!"));
        delete ctrl; 
        return;
      }
      
      ctrl->setName(cName);
      
      workingInstrument.controller()->add(ctrl);   
      QTreeWidgetItem* item = addControllerToView(ctrl);

      if(viewController->currentItem() != item)
      {
        viewController->blockSignals(true);
        viewController->setCurrentItem(item);
        viewController->blockSignals(false);
        controllerChanged();
      }

      workingInstrument.setDirty(true);
      }

//---------------------------------------------------------
//   addControllerClicked
//---------------------------------------------------------

void EditInstrument::addControllerClicked()
{
  // Add Common Controls not already found in instrument:
  PopupMenu* pup = new PopupMenu(true);  // true = enable stay open. Don't bother with parent.
  MusECore::MidiControllerList* cl = workingInstrument.controller();
  for(int num = 0; num < 127; ++num)
  {
    // If it's not already in the parent menu...
    if(cl->find(num) == cl->end())
      pup->addAction(MusECore::midiCtrlName(num, true))->setData(num);
  }
  connect(pup, SIGNAL(triggered(QAction*)), SLOT(ctrlPopupTriggered(QAction*)));
  pup->exec(mapToGlobal(QPoint(0,0)));
  delete pup;
}

//---------------------------------------------------------
//   ctrlPopupTriggered
//---------------------------------------------------------

void EditInstrument::ctrlPopupTriggered(QAction* act)
{
  if(!act || (act->data().toInt() == -1))
    return;
  int rv = act->data().toInt();
  MusECore::MidiControllerList* cl = workingInstrument.controller();
  if(cl->find(rv) == cl->end())
  {
    int num = rv; // = MusECore::MidiController::genNum(MusECore::MidiController::Controller7, 0, rv);
    MusECore::MidiController* ctrl = new MusECore::MidiController();
    ctrl->setNum(num);
    ctrl->setMinVal(0);
    ctrl->setMaxVal(127);
    ctrl->setInitVal(MusECore::CTRL_VAL_UNKNOWN);
    ctrl->setName(MusECore::midiCtrlName(num, false));
    
    workingInstrument.controller()->add(ctrl);

    QTreeWidgetItem* item = addControllerToView(ctrl);

    if(viewController->currentItem() != item)
    {
      viewController->blockSignals(true);
      viewController->setCurrentItem(item);
      viewController->blockSignals(false);
      controllerChanged();
    }

    workingInstrument.setDirty(true);
  }
}

//---------------------------------------------------------
//   updatePatchGroup
//---------------------------------------------------------

void EditInstrument::updatePatchGroup(MusECore::MidiInstrument* instrument, MusECore::PatchGroup* pg)
      {
	QString a = pg->name;
	QString b = patchNameEdit->text();
      if (pg->name != patchNameEdit->text()) {
            pg->name = patchNameEdit->text();
            instrument->setDirty(true);
            }
      }

//---------------------------------------------------------
//   updatePatch
//---------------------------------------------------------

void EditInstrument::updatePatch(MusECore::MidiInstrument* instrument, MusECore::Patch* p)
      {
      if (p->name != patchNameEdit->text()) {
            p->name = patchNameEdit->text();
            instrument->setDirty(true);
            }
      
      signed char hb = (spinBoxHBank->value() - 1) & 0xff;
      if (p->hbank != hb) {
            p->hbank = hb;
            
            instrument->setDirty(true);
            }
      
      signed char lb = (spinBoxLBank->value() - 1) & 0xff;
      if (p->lbank != lb) {
            p->lbank = lb;
            
            instrument->setDirty(true);
            }
      
      signed char pr = (spinBoxProgram->value() - 1) & 0xff;
      if (p->prog != pr) {
            p->prog = pr;
            
            instrument->setDirty(true);
            }
            
      if (p->drum != checkBoxDrum->isChecked()) {
            p->drum = checkBoxDrum->isChecked();
            instrument->setDirty(true);
            }
      
      // there is no logical xor in c++
// REMOVE Tim. OBSOLETE. When gui boxes are finally removed.
//       bool a = p->typ & 1;
//       bool b = p->typ & 2;
//       bool c = p->typ & 4;
//       bool aa = checkBoxGM->isChecked();
//       bool bb = checkBoxGS->isChecked();
//       bool cc = checkBoxXG->isChecked();
//       if ((a ^ aa) || (b ^ bb) || (c ^ cc)) {
//             int value = 0;
//             if (checkBoxGM->isChecked())
//                   value |= 1;
//             if (checkBoxGS->isChecked())
//                   value |= 2;
//             if (checkBoxXG->isChecked())
//                   value |= 4;
//             p->typ = value;
//             instrument->setDirty(true);
//             }
      }

//---------------------------------------------------------
//   updateInstrument
//---------------------------------------------------------

void EditInstrument::updateInstrument(MusECore::MidiInstrument* instrument)
      {
      QListWidgetItem* sysexItem = sysexList->currentItem();
      if (sysexItem) {
            MusECore::SysEx* so = (MusECore::SysEx*)sysexItem->data(Qt::UserRole).value<void*>();
            updateSysex(instrument, so);
            }

      QTreeWidgetItem* patchItem = patchView->currentItem();
      if (patchItem) 
      {      
        // If the item has a parent, it's a patch item.
        if(patchItem->parent())
          updatePatch(instrument, (MusECore::Patch*)patchItem->data(0, Qt::UserRole).value<void*>());
        else
          updatePatchGroup(instrument, (MusECore::PatchGroup*)patchItem->data(0, Qt::UserRole).value<void*>());
              
      }
    }

//---------------------------------------------------------
//   checkDirty
//    return true on Abort
//---------------------------------------------------------

int EditInstrument::checkDirty(MusECore::MidiInstrument* i, bool isClose)
      {
      updateInstrument(i);
      if (!i->dirty())
            return 0;

      int n;
      if(isClose) 
        n = QMessageBox::warning(this, tr("MusE"),
         tr("The current Instrument contains unsaved data\n"
         "Save Current Instrument?"),
         tr("&Save"), tr("&Nosave"), tr("&Abort"), 0, 2);
      else   
        n = QMessageBox::warning(this, tr("MusE"),
         tr("The current Instrument contains unsaved data\n"
         "Save Current Instrument?"),
         tr("&Save"), tr("&Nosave"), 0, 1);
      if (n == 0) {
            if (i->filePath().isEmpty())
            {
                  saveAs();
            }      
            else {
                  FILE* f = fopen(i->filePath().toLatin1().constData(), "w");
                  if(f == 0)
                        saveAs();
                  else {
                        if(fclose(f) != 0)
                          printf("EditInstrument::checkDirty: Error closing file\n");
                          
                        if(fileSave(i, i->filePath()))
                              i->setDirty(false);
                        }
                  }
            return 0;
            }
      return n;
      }

//---------------------------------------------------------
//    getPatchItemText
//---------------------------------------------------------

QString EditInstrument::getPatchItemText(int val)
{
  QString s;
  if(val == MusECore::CTRL_VAL_UNKNOWN)
    s = "---";
  else
  {
    int hb = ((val >> 16) & 0xff) + 1;
    if (hb == 0x100)
      hb = 0;
    int lb = ((val >> 8) & 0xff) + 1;
    if (lb == 0x100)
      lb = 0;
    int pr = (val & 0xff) + 1;
    if (pr == 0x100)
      pr = 0;
    s.sprintf("%d-%d-%d", hb, lb, pr);
  } 
  
  return s;
}                 

//---------------------------------------------------------
//    enableDefaultControls
//---------------------------------------------------------

void EditInstrument::enableDefaultControls(bool enVal, bool enPatch)
{
  spinBoxDefault->setEnabled(enVal);
  patchButton->setEnabled(enPatch);
  if(!enPatch)
  {
    patchButton->blockSignals(true);
    patchButton->setText("---");
    patchButton->blockSignals(false);
  }
  defPatchH->setEnabled(enPatch);
  defPatchL->setEnabled(enPatch);
  defPatchProg->setEnabled(enPatch);
}

//---------------------------------------------------------
//    enableNonCtrlControls
//---------------------------------------------------------

void EditInstrument::enableNonCtrlControls(bool v)
{
  QTreeWidgetItem* sel = viewController->selectedItems().size() ? viewController->selectedItems()[0] : 0;

  if(!sel || !sel->data(0, Qt::UserRole).value<void*>())
    return;
  MusECore::MidiController* c = (MusECore::MidiController*)sel->data(0, Qt::UserRole).value<void*>();
  MusECore::MidiController::ControllerType type = MusECore::midiControllerType(c->num());

  if(v)
  {
    switch (type) {
        case MusECore::MidiController::Controller7:
              spinBoxMin->setEnabled(true);
              spinBoxMax->setEnabled(true);
              enableDefaultControls(true, false);
              break;
        case MusECore::MidiController::RPN:
        case MusECore::MidiController::NRPN:
              spinBoxMin->setEnabled(true);
              spinBoxMax->setEnabled(true);
              enableDefaultControls(true, false);
              break;
        case MusECore::MidiController::Controller14:
        case MusECore::MidiController::RPN14:
        case MusECore::MidiController::NRPN14:
              spinBoxMin->setEnabled(true);
              spinBoxMax->setEnabled(true);
              enableDefaultControls(true, false);
              break;
        case MusECore::MidiController::Pitch:
              spinBoxMin->setEnabled(true);
              spinBoxMax->setEnabled(true);
              enableDefaultControls(true, false);
              break;
        case MusECore::MidiController::PolyAftertouch:
        case MusECore::MidiController::Aftertouch:
              spinBoxMin->setEnabled(true);
              spinBoxMax->setEnabled(true);
              enableDefaultControls(true, false);
              break;
        case MusECore::MidiController::Program:
              spinBoxMin->setEnabled(false);
              spinBoxMax->setEnabled(false);
              enableDefaultControls(false, true);
              break;
        default:
              spinBoxMin->setEnabled(false);
              spinBoxMax->setEnabled(false);
              enableDefaultControls(false, false);
              break;
        }
  }
  else
  {
    spinBoxDefault->setEnabled(false);
    patchButton->setEnabled(false);
    defPatchH->setEnabled(false);
    defPatchL->setEnabled(false);
    defPatchProg->setEnabled(false);

    spinBoxMin->setEnabled(false);
    spinBoxMax->setEnabled(false);
  }
  
  ctrlShowInMidi->setEnabled(v);
  ctrlShowInDrum->setEnabled(v);

  ctrlName->setEnabled(v);
}

//---------------------------------------------------------
//    setDefaultPatchName
//---------------------------------------------------------

void EditInstrument::setDefaultPatchName(int val)
{
  patchButton->blockSignals(true);
  patchButton->setText(getPatchName(val));
  patchButton->blockSignals(false);
}

//---------------------------------------------------------
//    getDefaultPatchNumber
//---------------------------------------------------------

int EditInstrument::getDefaultPatchNumber()
{      
  int hval = defPatchH->value() - 1;
  int lval = defPatchL->value() - 1;
  int prog = defPatchProg->value() - 1;
  if(hval == -1)
    hval = 0xff;
  if(lval == -1)
    lval = 0xff;
  if(prog == -1)
    prog = 0xff;
    
  return ((hval & 0xff) << 16) + ((lval & 0xff) << 8) + (prog & 0xff); 
}
      
//---------------------------------------------------------
//    setDefaultPatchNumbers
//---------------------------------------------------------

void EditInstrument::setDefaultPatchNumbers(int val)
{
  int hb;
  int lb;
  int pr;
  
  if(val == MusECore::CTRL_VAL_UNKNOWN)
    hb = lb = pr = 0;
  else
  {
    hb = ((val >> 16) & 0xff) + 1;
    if (hb == 0x100)
      hb = 0;
    lb = ((val >> 8) & 0xff) + 1;
    if (lb == 0x100)
      lb = 0;
    pr = (val & 0xff) + 1;
    if (pr == 0x100)
      pr = 0;
  } 
    
  defPatchH->blockSignals(true);
  defPatchL->blockSignals(true);
  defPatchProg->blockSignals(true);
  defPatchH->setValue(hb);  
  defPatchL->setValue(lb);  
  defPatchProg->setValue(pr);
  defPatchH->blockSignals(false);
  defPatchL->blockSignals(false);
  defPatchProg->blockSignals(false);
}

//---------------------------------------------------------
//    setDefaultPatchControls
//---------------------------------------------------------

void EditInstrument::setDefaultPatchControls(int val)
{
  setDefaultPatchNumbers(val);
  setDefaultPatchName(val);
}

//---------------------------------------------------------
//   getPatchName
//---------------------------------------------------------

QString EditInstrument::getPatchName(int prog)
{
      int pr = prog & 0xff;
      if(prog == MusECore::CTRL_VAL_UNKNOWN || pr == 0xff)
            return "---";
      
      int hbank = (prog >> 16) & 0xff;
      int lbank = (prog >> 8) & 0xff;

      MusECore::PatchGroupList* pg = workingInstrument.groups();
      
      for(MusECore::ciPatchGroup i = pg->begin(); i != pg->end(); ++i) {
            const MusECore::PatchList& pl = (*i)->patches;
            for (MusECore::ciPatch ipl = pl.begin(); ipl != pl.end(); ++ipl) {
                  const MusECore::Patch* mp = *ipl;
                  if (//(mp->typ & tmask) && DELETETHIS
                    (pr == mp->prog)
                    //&& ((drum && mode != MT_GM) ||  DELETETHIS
                    //   (mp->drum == drumchan))   
                    
                    //&& (hbank == mp->hbank || !hb || mp->hbank == -1)
                    //&& (lbank == mp->lbank || !lb || mp->lbank == -1))
                    && (hbank == mp->hbank || mp->hbank == -1)
                    && (lbank == mp->lbank || mp->lbank == -1))
                        return mp->name;
                  }
            }
      return "---";
}

} // namespace MusEGui
