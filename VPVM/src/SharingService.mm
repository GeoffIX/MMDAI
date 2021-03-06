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

#include "SharingService.h"

#include <QDebug>
#include <QImage>
#include <QTemporaryFile>
#import <AppKit/AppKit.h>

namespace {

const NSString *kInitialMessageText = @"#MMDAI2ss";

}

QStringList SharingService::availableServiceNames()
{
    QStringList serviceNames;
    if (NSClassFromString(@"NSSharingService") != nil) {
        NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(0, 0)];
        NSArray *sharingItems = [[NSArray alloc] initWithObjects:kInitialMessageText, image, nil];
        NSArray *services = [NSSharingService sharingServicesForItems:sharingItems];
        [image release];
        [sharingItems release];
        for (NSSharingService *service in services) {
            if ([service canPerformWithItems:nil]) {
                serviceNames << QString([service.title UTF8String]);
            }
        }
    }
    return serviceNames;
}

SharingService::SharingService()
{
}

SharingService::~SharingService()
{
}

void SharingService::setServiceName(const QString &value)
{
    m_serviceName = value;
}

void SharingService::showPostForm(const QImage &image)
{
    QTemporaryFile file;
    if (NSClassFromString(@"NSSharingService") != nil && !m_serviceName.isNull() && file.open()) {
        image.save(&file, "TIFF");
        NSString *filePath = [[NSString alloc] initWithUTF8String:file.fileName().toUtf8().constData()];
        NSImage *attachImage = [[NSImage alloc] initWithContentsOfFile:filePath];
        NSArray *sharingItems = [[NSArray alloc] initWithObjects:kInitialMessageText, attachImage, nil];
        [filePath release];
        [attachImage release];
        NSSharingService *serviceObjectRef = nil;
        NSString *serviceName = [[NSString alloc] initWithUTF8String:m_serviceName.toUtf8().constData()];
        NSArray *services = [NSSharingService sharingServicesForItems:sharingItems];
        for (NSSharingService *service in services) {
            if ([serviceName isEqualToString:service.title]) {
                serviceObjectRef = service;
            }
        }
        [serviceName release];
        if (serviceObjectRef != nil) {
            [serviceObjectRef performWithItems:sharingItems];
        }
        [sharingItems release];
    }
}
