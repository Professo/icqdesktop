//
//  WindowTest.h
//  ICQ
//
//  Created by IceSeer on 04/12/15.
//  Copyright © 2015 Mail.RU. All rights reserved.
//

#ifndef __WINDOW_TEST_H__
#define __WINDOW_TEST_H__

#include <QtWidgets/qwidget.h>

namespace platform_specific
{
    
    class GraphicsPanel : public QWidget
    {
    public:
        static GraphicsPanel* create(QWidget* _parent, std::vector<QWidget*>& _panels);

        GraphicsPanel(QWidget* _parent) : QWidget(_parent) {}
        virtual ~GraphicsPanel() { }
        
        virtual WId frameId() const;

        virtual void addPanels(std::vector<QWidget*>& _panels);
        virtual void clearPanels();
        virtual void fullscreenModeChanged(bool _fullscreen);
        virtual void fullscreenAnimationStart();
        virtual void fullscreenAnimationFinish();
    };
    
}

#endif//__WINDOW_TEST_H__
