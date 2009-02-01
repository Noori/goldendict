/* This file is (c) 2008-2009 Konstantin Isakov <ikm@users.sf.net>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "scanpopup.hh"
#include "folding.hh"
#include <QUrl>
#include <QCursor>
#include <QPixmap>
#include <QBitmap>
#include <QMenu>

using std::wstring;

ScanPopup::ScanPopup( QWidget * parent,
                      ArticleNetworkAccessManager & articleNetMgr,
                      std::vector< sptr< Dictionary::Class > > const & allDictionaries_,
                      Instances::Groups const & groups_ ):
  QDialog( parent ),
  allDictionaries( allDictionaries_ ),
  groups( groups_ ),
  wordFinder( this )
{
  ui.setupUi( this );
  definition = new ArticleView( ui.outerFrame, articleNetMgr, groups, true ),
  ui.mainLayout->addWidget( definition );

  ui.diacriticButton->hide();
  ui.prefixButton->hide();

  ui.groupList->fill( groups );
  setWindowFlags( Qt::Popup );

  #if 0 // Experimental code to give window a non-rectangular shape (i.e.
        // balloon) using a colorkey mask.
  QPixmap pixMask( size() );
  render( &pixMask );

  setMask( pixMask.createMaskFromColor( QColor( 255, 0, 0 ) ) );

  // This helps against flickering
  setAttribute( Qt::WA_NoSystemBackground );
  #endif

  connect( ui.groupList, SIGNAL( currentIndexChanged( QString const & ) ),
           this, SLOT( currentGroupChanged( QString const & ) ) );

  connect( wordFinder.qobject(), SIGNAL( prefixMatchComplete( WordFinderResults ) ),
           this, SLOT( prefixMatchComplete( WordFinderResults ) ) );

  connect( ui.word, SIGNAL( clicked() ),
           this, SLOT( initialWordClicked() ) );
  connect( ui.diacriticButton, SIGNAL( clicked() ),
           this, SLOT( diacriticButtonClicked() ) );
  connect( ui.prefixButton, SIGNAL( clicked() ),
           this, SLOT( prefixButtonClicked() ) );

  connect( ui.pinButton, SIGNAL( clicked( bool ) ),
           this, SLOT( pinButtonClicked( bool ) ) );

  connect( QApplication::clipboard(), SIGNAL( changed( QClipboard::Mode ) ),
           this, SLOT( clipboardChanged( QClipboard::Mode ) ) );
}

void ScanPopup::clipboardChanged( QClipboard::Mode m )
{
  printf( "clipboard changed\n" );

  // Check key modifiers

  if ( !checkModifiersPressed( Win ) )
    return;

  QString subtype = "plain";

  inputWord = QApplication::clipboard()->text( subtype, m ).trimmed();

  if ( !inputWord.size() )
    return;

  setWindowTitle( inputWord );
  ui.word->setText( inputWord );

  if ( !isVisible() )
  {
    QPoint currentPos = QCursor::pos();

    move( currentPos.x() + 4, currentPos.y() + 10 );

    show();

    QApplication::processEvents(); // Make window appear immediately no matter what
  }

  initiateTranslation();
}

void ScanPopup::currentGroupChanged( QString const & )
{
  if ( isVisible() )
    initiateTranslation();
}

void ScanPopup::initiateTranslation()
{
  wordFinder.prefixMatch( inputWord, &getActiveDicts() );
}

vector< sptr< Dictionary::Class > > const & ScanPopup::getActiveDicts()
{
  int currentGroup = ui.groupList->currentIndex();

  return
    currentGroup < 0 || currentGroup >= (int)groups.size() ? allDictionaries : 
    groups[ currentGroup ].dictionaries;
}

void ScanPopup::leaveEvent( QEvent * event )
{
  QDialog::leaveEvent( event );

  // We hide the popup when the mouse leaves it. So in order to close it
  // without any clicking the cursor has to get inside and then to leave.
  
  // Combo-boxes seem to generate leave events for their parents when
  // unfolded, so we check coordinates as well.
  // If the dialog is pinned, we don't hide the popup
  if ( !ui.pinButton->isChecked() && !geometry().contains( QCursor::pos() ) )
    hide();
}

void ScanPopup::prefixMatchComplete( WordFinderResults r )
{
  // Check that the request wasn't already overridden by another one and
  // that there's a window there at all
  if ( isVisible() && r.requestStr == inputWord &&
       r.requestDicts == &getActiveDicts() )
  {
    // Find the matches that aren't prefix. If there're more than one,
    // show the diacritic toolbutton. If there are prefix matches, show
    // the prefix toolbutton.

    diacriticMatches.clear();
    prefixMatches.clear();

    wstring foldedInputWord = Folding::apply( inputWord.toStdWString() );

    for( unsigned x = 0; x < r.results.size(); ++x )
    {
      if ( Folding::apply( r.results[ x ].toStdWString() ) == foldedInputWord )
        diacriticMatches.push_back( r.results[ x ] );
      else
        prefixMatches.push_back( r.results[ x ] );
    }

    if ( diacriticMatches.size() > 1 )
    {
      ui.diacriticButton->setToolTip( tr( "%1 results differing in diacritic marks" ).arg( diacriticMatches.size() ) );
      ui.diacriticButton->show();
    }
    else
      ui.diacriticButton->hide();

    if ( prefixMatches.size() )
    {
      ui.prefixButton->setToolTip( tr( "%1 result(s) beginning with the search word" ).arg( prefixMatches.size() ) );
      ui.prefixButton->show();
    }
    else
      ui.prefixButton->hide();

    if ( diacriticMatches.size() )
      definition->showDefinition( diacriticMatches[ 0 ], ui.groupList->currentText() );
    else
    {
      // No matches
      definition->showNotFound( inputWord, ui.groupList->currentText() );
    }
  }
}

void ScanPopup::diacriticButtonClicked()
{
  popupWordlist( diacriticMatches, ui.diacriticButton );
}

void ScanPopup::prefixButtonClicked()
{
  popupWordlist( prefixMatches, ui.prefixButton );
}

void ScanPopup::popupWordlist( vector< QString > const & words, QToolButton * button )
{
  if ( !isVisible() )
    return;

  if ( words.empty() )
    return;

  QMenu menu( this );

  for( unsigned x = 0; x < words.size(); ++x )
    menu.addAction( words[ x ] );

  QAction * result = menu.exec( mapToGlobal( button->pos() ) +
                                  QPoint( 0, button->height() ) );

  if ( result )
    definition->showDefinition( result->text(), ui.groupList->currentText() );
}

void ScanPopup::initialWordClicked()
{
  if ( isVisible() && diacriticMatches.size() )
    definition->showDefinition( diacriticMatches[ 0 ], ui.groupList->currentText() );
}

void ScanPopup::pinButtonClicked( bool checked )
{
  if ( checked )
    setWindowFlags( Qt::Dialog  );
  else
    setWindowFlags( Qt::Popup );

  // Should we disable grip? I like it with the grip better.
  //ui.gripArea->setDisabled( checked );

  show();
}